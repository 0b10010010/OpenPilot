/**
 * \file hardwareSensorGpsGenom.hpp
 *
 * Header file for getting data from the genom gps module
 *
 * \date 16/03/2011
 * \author croussil
 *
 * \ingroup rtslam
 */

#ifndef HARDWARE_SENSOR_GPSGENOM_HPP_
#define HARDWARE_SENSOR_GPSGENOM_HPP_

#include <stdlib.h>
#include <unistd.h>

#include <jafarConfig.h>
#include "kernel/jafarMacro.hpp"
#include "rtslam/hardwareSensorAbstract.hpp"

#ifdef HAVE_POSTERLIB
#include "posterLib.h"
#endif


namespace jafar {
namespace rtslam {
namespace hardware {


class HardwareSensorGpsGenom: public HardwareSensorProprioAbstract
{
	private:
		boost::thread *preloadTask_thread;
		void preloadTask(void);
		
#ifdef HAVE_POSTERLIB
		POSTER_ID posterId;
#endif
		RawVec reading;
		int mode;
		std::string dump_path;
		double last_timestamp;
		
	public:
		HardwareSensorGpsGenom(kernel::VariableCondition<int> &condition, unsigned bufferSize, const std::string machine, int mode = 0, std::string dump_path = ".");
		
		virtual double getLastTimestamp() { boost::unique_lock<boost::mutex> l(mutex_data); return last_timestamp; }
		virtual int dataSize() { return 3; }
		virtual int varianceSize() { return 3; }
		
};


}}}

#endif // HARDWARESENSORABSLOCGPSGENOM_HPP
