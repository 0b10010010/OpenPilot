/**
 * \file hardwareEstimatorAbstract.hpp
 *
 * Header file for hardware robots
 *
 * \author croussil@laas.fr
 * \date 18/06/2010
 *
 * \ingroup rtslam
 */

#ifndef HARDWARE_ESTIMATOR_ABSTRACT_HPP_
#define HARDWARE_ESTIMATOR_ABSTRACT_HPP_

namespace jafar {
namespace rtslam {
namespace hardware {

	class HardwareEstimatorAbstract;
	typedef boost::shared_ptr<HardwareEstimatorAbstract> hardware_estimator_ptr_t;


	class HardwareEstimatorAbstract
	{
		public:
			/**
			Returns all reading between t1 and t2, plus the last before t1 and the first after t2.
			Each line is one reading, first element is timestamp (double seconds), the rest is the command.
			*/
			virtual jblas::mat_indirect acquireReadings(double t1, double t2) = 0;
			/**
			If acquireReadings blocked some values the time they are used, this function must release them.
			*/
			virtual void releaseReadings() = 0;
			/**
			This function must return the indices of values returned by acquireReadings that represent
			the instant value of a physical quantity (speed, acceleration, ...) (except first column which is time)
			These values will typically be integrated.
			*/
			virtual jblas::ind_array instantValues() = 0;
			/**
			This function must return the indices of values returned by acquireReadings that represent
			the increment of a physical quantity since last reading (odometry, ...)
			*/
			virtual jblas::ind_array incrementValues() = 0;
	};

}}}

#endif

