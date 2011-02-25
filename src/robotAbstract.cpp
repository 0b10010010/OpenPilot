/**
 * \file robotAbstract.cpp
 * \date 08/03/2010
 * \author jsola
 * \ingroup rtslam
 */

#include "rtslam/robotAbstract.hpp"
#include "rtslam/sensorAbstract.hpp"
#include "rtslam/mapAbstract.hpp"

#include <boost/shared_ptr.hpp>

namespace jafar {
	namespace rtslam {
		using namespace std;

		IdFactory RobotAbstract::robotIds = IdFactory();

		/*
		 * Operator << for class RobotAbstract.
		 * It shows different information of the robot.
		 */
		ostream& operator <<(ostream & s, RobotAbstract const & rob) {
			s << rob.categoryName() << " " << rob.id() << ": ";
			if (rob.name().size() > 0) s << rob.name() << ", ";
			s << "of type " << rob.typeName() << endl;
			s << ".state:  " << rob.state << endl;
//			s << ".pose :  " << rob.pose << endl;
			s << ".st.pert:   " << rob.Q << endl;
			s << ".sensor list : [";
			for (RobotAbstract::SensorList::const_iterator senIter = rob.sensorList().begin();
						senIter != rob.sensorList().end(); senIter++)
				s << " " << (*senIter)->id() << " "; // Print the address of the sensor.
			s << "]";
			return s;
		}


		/*
		 * Remote constructor from remote map and size of control vector.
		 */
		RobotAbstract::RobotAbstract(const map_ptr_t & _mapPtr, const size_t _size_state, const size_t _size_control, const size_t _size_pert) :
			MapObject(_mapPtr, _size_state),
			pose(state, jmath::ublasExtra::ia_set(0, 7)),
			control(_size_control),
			perturbation(_size_pert),
			XNEW_x(_size_state, _size_state),
			XNEW_pert(_size_state, _size_pert),
			Q(_size_state, _size_state)
		{
			constantPerturbation = false;
			category = ROBOT;
			self_time = -1.;
			Q.clear();
		}

		RobotAbstract::RobotAbstract(const simulation_t dummy, const map_ptr_t & _mapPtr, const size_t _size_state, const size_t _size_control, const size_t _size_pert) :
			MapObject(_mapPtr, _size_state, UNFILTERED),
			pose(state, jmath::ublasExtra::ia_set(0, 7)),
			control(_size_control),
			perturbation(_size_pert),
			XNEW_x(_size_state, _size_state),
			XNEW_pert(_size_state, _size_pert),
			Q(_size_state, _size_state)
		{
			constantPerturbation = true;
			category = ROBOT;
			self_time = -1.;
		}


		void RobotAbstract::computeStatePerturbation() {
			Q = jmath::ublasExtra::prod_JPJt(perturbation.P(), XNEW_pert);
//JFR_DEBUG("P " << perturbation.P());
//JFR_DEBUG("XNEW_pert " << XNEW_pert);
//JFR_DEBUG("Q " << Q);
		}

		void RobotAbstract::move(double time){
			bool firstmove = false;
			if (self_time < 0.) { firstmove = true; self_time = time; }
			if (hardwareEstimatorPtr)
			{
//firstmove=false;
				if (firstmove) // compute average past control and allow the robot to init its state with it
				{
					jblas::mat_indirect readings = hardwareEstimatorPtr->acquireReadings(0, time);
					self_time = 0.;
					dt_or_dx = 0.;
					jblas::vec avg_u(readings.size2()-1); avg_u.clear();
					unsigned nreadings = readings.size1();
					if (readings(nreadings-1, 0) >= time) nreadings--; // because it could be available offline but not online
					for(size_t i = 0; i < nreadings; i++)
						avg_u += ublas::subrange(ublas::matrix_row<mat_indirect>(readings, i),1,readings.size2());
					
					if (nreadings) avg_u /= nreadings;
					init(avg_u);
				}
				else // else just move with the available control
				{
					jblas::mat_indirect readings = hardwareEstimatorPtr->acquireReadings(self_time, time);
// JFR_DEBUG("move from " << std::setprecision(19) << self_time << " to " << time << " with cur_time " << self_time << std::setprecision(6) << " using " << readings.size1() << " readings");
					jblas::vec u(readings.size2()-1), prev_u(readings.size2()-1), next_u(readings.size2()-1);
					
					jblas::ind_array instantArray = hardwareEstimatorPtr->instantValues()-1;
					jblas::ind_array incrementArray = hardwareEstimatorPtr->incrementValues()-1;
					jblas::vec_indirect u_instant(u, instantArray), prev_u_instant(prev_u, instantArray), next_u_instant(next_u, instantArray);
					jblas::vec_indirect u_increment(u, incrementArray), prev_u_increment(prev_u, incrementArray), next_u_increment(next_u, incrementArray);
					
					double a, cur_time = self_time, after_time, prev_time = readings(0, 0), next_time, average_time;
					prev_u = ublas::subrange(ublas::matrix_row<mat_indirect>(readings, 0),1,readings.size2());
				
					for(size_t i = 0; i < readings.size1(); i++)
					{
						next_time = after_time = readings(i, 0);
						if (after_time > time || i == readings.size1()-1) after_time = time;
						if (after_time <= cur_time) continue;
						dt_or_dx = after_time - cur_time;
						perturbation.set_from_continuous(dt_or_dx);
						next_u = ublas::subrange(ublas::matrix_row<mat_indirect>(readings, i),1,readings.size2());
						
						average_time = (after_time+cur_time)/2; // middle of the integration interval
						if (next_time-prev_time < 1e-6) a = 0; else a = (average_time-prev_time)/(next_time-prev_time);
						u_instant = (1-a)*prev_u_instant + a*next_u_instant; // average command for the integration interval
						
						if (next_time-prev_time < 1e-6) a = 0; else a = (after_time-prev_time)/(next_time-prev_time);
						u_increment = a*next_u_increment;
//JFR_DEBUG("elementary move between " << std::setprecision(19) << cur_time << " and " << after_time << " (dt " << dt_or_dx << std::setprecision(6) << ") with command " << u << " between " << prev_u << " at " << std::setprecision(19) << prev_time << std::setprecision(6) << " (" << (1-a) << ") and " << next_u << " at " << std::setprecision(19) << next_time << std::setprecision(6) << " (" << a << ")");
						move(u);
						
						prev_time = cur_time = next_time;
						prev_u = next_u;
					}
					dt_or_dx = time - self_time;
				}
			} else
			{
				dt_or_dx = time - self_time;
				perturbation.set_from_continuous(dt_or_dx);
				control.clear();
				move();
			}
			self_time = time;
		}


		void RobotAbstract::writeLogHeader(kernel::DataLogger& log) const
		{
			std::ostringstream oss; oss << "Robot " << id();
			log.writeComment(oss.str());
			log.writeLegendTokens("time");
			for(size_t i = 0; i < state.x().size(); ++i)
				{ oss.str(""); oss << "x" << i; log.writeLegend(oss.str()); }
			for(size_t i = 0; i < state.x().size(); ++i)
				{ oss.str(""); oss << "sig" << i; log.writeLegend(oss.str()); }
		}
		
		void RobotAbstract::writeLogData(kernel::DataLogger& log) const
		{
			log.writeData(self_time);
			for(size_t i = 0; i < state.x().size(); ++i)
				log.writeData(state.x()(i));
			for(size_t i = 0; i < state.x().size(); ++i)
				log.writeData(sqrt(state.P()(i,i)));
		}


	}
}
