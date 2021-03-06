#include "skillSet.h"
#include <navigation/planners.h>
#include <navigation/controllers/waypoint.h>
// #include "logger.h"
// #include "timer.h"
#include <ssl_common/config.h>
#include <ssl_common/grSimComm.h>

// static Util::Timer  timer;
// extern Util::Logger logger;

//Add comm.addLine & comm.addCircle
#define POINTPREDICTIONFACTOR 2

using namespace std;

namespace Strategy
{
  gr_Robot_Command SkillSet::goToBall(const SParam &param, const BeliefState &state, int botID)
  {
    Vector2D<int> botPos(state.homePos[botID].x, state.homePos[botID].y);
#if 1
    using Navigation::obstacle;
    vector<obstacle> obs;
    obstacle o;
    for (int i = 0; i < state.homeDetected.size(); ++i)
    {
      o.x = state.homePos[i].x;
      o.y = state.homePos[i].y;
      o.radius = 2 * BOT_RADIUS;
      obs.push_back(o);
    }

    for (int i = state.homeDetected.size(); i < state.homeDetected.size() + state.awayDetected.size(); ++i)
    {
      o.x = state.awayPos[i - state.homeDetected.size()].x;
      o.y = state.awayPos[i - state.homeDetected.size()].y;
      o.radius = 2 * BOT_RADIUS;
      obs.push_back(o);
    }
    Vector2D<int> ballfinalpos;
    ballfinalpos.x = state.ballPos.x + (state.ballVel.x / POINTPREDICTIONFACTOR);
    ballfinalpos.y = state.ballPos.y + (state.ballVel.y / POINTPREDICTIONFACTOR);
    Vector2D<int> point, nextWP, nextNWP;

    // use the mergescurver planner
    Navigation::MergeSCurve pathPlanner;
    pathPlanner.plan(botPos,
                      ballfinalpos,
                      &nextWP,
                      &nextNWP,
                      &obs[0],
                      obs.size(),
                      botID,
                      true);

    if (nextWP.valid())
    {
      // comm.addCircle(nextWP.x, nextWP.y, 50);
      // comm.addLine(state.homePos[botID].x, state.homePos[botID].y, nextWP.x, nextWP.y);
    }
    if (nextNWP.valid())
    {
      // comm.addCircle(nextNWP.x, nextNWP.y, 50);
      // comm.addLine(nextWP.x, nextWP.y, nextNWP.x, nextNWP.y);
    }
#else
    vector<ERRT::obstacle> obs;
    ERRT::obstacle o;
    for (int i = 0; i < state.homeDetected.size(); ++i)
    {
      if (i != botID)
      {
        o.center = Point2D<int>(state.homePos[i].x, state.homePos[i].y);
        o.radius = 2.2f * BOT_RADIUS;
        obs.push_back(o);
        comm.addCircle(o.center.x, o.center.y, o.radius);
      }
    }

    for (int i = state.homeDetected.size(); i < state.homeDetected.size() + state.awayDetected.size(); ++i)
    {
      o.center = Point2D<int>(state.awayPos[i - state.homeDetected.size()].x, state.awayPos[i - state.homeDetected.size()].y);
      o.radius = 2.2f * BOT_RADIUS;
      obs.push_back(o);
      comm.addCircle(o.center.x, o.center.y, o.radius);
    }

    list<Point2D<int> > waypoints;
    timer.start();
    bool found = errt->plan(botPos, state.ballPos, obs, 100, waypoints);
    logger.add("%d us", timer.stopus());

    Vector2D<int> nextWP, nextNWP;

    Point2D<int> point1 = botPos;
    int counter = 0;
    while (waypoints.empty() == false)
    {
      Point2D<int> point2 = waypoints.front();
      waypoints.pop_front();
      if (found && waypoints.empty())
      {
        comm.addCircle(point2.x, point2.y, 150);  // marking the end point of the path, when one is found
      }
      if (counter == 0 && Point2D<int>::distSq(botPos, point2) > BOT_RADIUS * BOT_RADIUS * 2)
      {
        nextWP = point2;
        ++counter;
      }
      else if (counter == 1)
      {
        nextNWP = point2;
        ++counter;
      }
      comm.addCircle(point1.x, point1.y, 30);
      comm.addLine(point1.x, point1.y, point2.x, point2.y);
      point1 = point2;
    }

    return getRobotCommandMessage(botID, 0, 0, 0, 0, false);
#endif

#if 1
    float motionAngle = Vector2D<int>::angle(nextWP, botPos);

    float finalSlope;   // final slope the current bot motion should aim for!
    if(nextNWP.valid())
      finalSlope = Vector2D<int>::angle(nextNWP, nextWP);
    else
      finalSlope = Vector2D<int>::angle(nextWP, botPos);

    float turnAngleLeft = normalizeAngle(finalSlope - state.homePos[botID].theta); // Angle left to turn

    float omega = turnAngleLeft * MAX_BOT_OMEGA / (2 * PI); // Speedup turn
    if(omega < MIN_BOT_OMEGA && omega > -MIN_BOT_OMEGA)
    {
      if(omega < 0) omega = -MIN_BOT_OMEGA;
      else omega = MIN_BOT_OMEGA;
    }

    float dist = Vector2D<int>::dist(nextWP, botPos);  // Distance of next waypoint from the bot
    float theta =  motionAngle - state.homePos[botID].theta;               // Angle of dest with respect to bot's frame of reference

    float profileFactor = (dist * 4 / MAX_FIELD_DIST) * MAX_BOT_SPEED;

    if(profileFactor < MIN_BOT_SPEED && dist > BOT_BALL_THRESH)
      profileFactor = MIN_BOT_SPEED;
    else if(profileFactor > MAX_BOT_SPEED)
      profileFactor = MAX_BOT_SPEED;
#ifdef GR_SIM_COMM
    if(dist < BOT_BALL_THRESH * 5)
      profileFactor = dist / MAX_FIELD_DIST * MAX_BOT_SPEED;
#endif
    if(param.GoToBallP.intercept == false)
    {
      if (dist < DRIBBLER_BALL_THRESH)
      {
        if(dist < BOT_BALL_THRESH)
        {
          if((turnAngleLeft) > -DRIBBLER_BALL_ANGLE_RANGE  && (turnAngleLeft) < DRIBBLER_BALL_ANGLE_RANGE)
          {
//            printf("not moving\n");
              return getRobotCommandMessage(botID, 0, 0, 0, 0, true);
          }
          else
          {
//            printf("turning\n");
              return getRobotCommandMessage(botID, 0, 0, omega, 0, true);
          }
        }
        else
        {
          // Make the dribbler on
//				if ((turnAngleLeft > -DRIBBLER_BALL_ANGLE_RANGE) && (turnAngleLeft < DRIBBLER_BALL_ANGLE_RANGE))
//				{
//          printf("not moving");
//					return getRobotCommandMessage(botID, 0, 0, 0, 0, true);
//				}
//				else
          {
//            printf("Moving: %f, %f\n", profileFactor, omega);
              return getRobotCommandMessage(botID, profileFactor * sin(-theta), profileFactor * cos(-theta), omega, 0, true);
          }
        }
      }
      else
      {
//        printf("Moving: %f, %f\n", profileFactor, omega);
          return getRobotCommandMessage(botID, profileFactor * sin(-theta), profileFactor * cos(-theta), omega, 0, false);
      }
    }
    else
    {
      if(dist > BOT_BALL_THRESH)
      {
//        printf("Moving: %f, %f\n", profileFactor, omega);
          return getRobotCommandMessage(botID, profileFactor * sin(-theta), profileFactor * cos(-theta), 0, 0, false);      
      }
      else
      {
//        printf("Moving: %f, %f\n", profileFactor, omega);
          return getRobotCommandMessage(botID, 0, 0, 0, 0, true);
      }

    }

#else
    return getRobotCommandMessage(botID, 0, 0, 0, 0, false);
#endif
  } // goToBall
}
