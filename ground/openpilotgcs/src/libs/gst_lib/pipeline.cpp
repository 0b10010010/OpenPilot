/*
 * pipeline.cpp
 *
 *  Created on: 15 d�c. 2012
 *      Author: Utilisateur
 */

#include "pipeline.h"

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>

Pipeline::Pipeline() {
	// initialize gstreamer
	gst::init(NULL, NULL);
}

Pipeline::~Pipeline() {
}

