/**
 ******************************************************************************
 *
 * @file       videowidget.cpp
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "videowidget.h"

#include <QtCore>
#include <QPainter>
#include <QDebug>
#include <QRect>
#include <QTextDocument>
#include <QApplication>

#include <string>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include "overlay.h"
#include "pipelineevent.h"

class GstOverlayImpl: public Overlay {
public:
    GstOverlayImpl(GstVideoOverlay *gst_overlay) :
            gst_overlay(gst_overlay)
    {
    }
    void expose()
    {
        if (gst_overlay) {
            gst_video_overlay_expose(gst_overlay);
        }
    }
private:
    GstVideoOverlay *gst_overlay;
};

class BusSyncHandler {
public:
    BusSyncHandler(QWidget *widget, WId wid) :
            widget(widget), wId(wid)
    {
    }
    bool handleMessage(GstMessage *msg);
private:
    QWidget *widget;
    WId wId;

};

static GstElement *createPipelineFromDesc(const char*, QString &lastError);
//static GstElement *createTestPipeline();

static GstBusSyncReply gst_bus_sync_handler(GstBus *, GstMessage *, BusSyncHandler *);

VideoWidget::VideoWidget(QWidget *parent) :
        QWidget(parent), pipeline(NULL), overlay(NULL)
{
    qDebug() << QString("VideoWidget - creating (%0) - #%1").arg((long) QThread::currentThreadId()).arg("VideoWidget");

    // initialize gstreamer
    gst::init(NULL, NULL);

    // make the widget native so it gets its own native window id that we will pass to gstreamer
    setAttribute(Qt::WA_NativeWindow);
    //setAttribute(Qt::WA_DontCreateNativeAncestors);

    // set black background
    QPalette pal(palette());
    pal.setColor(backgroundRole(), Qt::black);
    setPalette(pal);

    WId wid = winId();
    qDebug() << QString("VideoWidget - video winId : %0").arg((gulong) wid);
    handler = new BusSyncHandler(this, wid);

    // init widget state (see setOverlay() for more information)
    //setOverlay(NULL);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_PaintOnScreen, false);

    // init state
    lastError = "";
    reset = false;

    qDebug() << QString("VideoWidget - created video widget winId : %0").arg((gulong) wid);
}

VideoWidget::~VideoWidget()
{
    dispose();
    if (handler) {
        delete handler;
        handler = NULL;
    }
}

bool VideoWidget::isPlaying()
{
    return (pipeline && GST_STATE(pipeline) == GST_STATE_PLAYING);
}

void VideoWidget::setPipelineDesc(QString pipelineDesc)
{
    qDebug() << QString("VideoWidget - setting pipeline desc to : %0").arg(pipelineDesc);
    this->pipelineDesc = pipelineDesc;
}

void VideoWidget::start()
{
    qDebug() << QString("VideoWidget - starting pipeline : %0").arg(pipelineDesc);
    init();
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    repaint();
}

void VideoWidget::pause()
{
    init();
    if (GST_STATE(pipeline) == GST_STATE_PAUSED) {
        gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    } else if (GST_STATE(pipeline) == GST_STATE_PLAYING) {
        gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
    }
    repaint();
}

void VideoWidget::stop()
{
    //gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    dispose();
    repaint();
}

void VideoWidget::init()
{
    if (reset) {
        qDebug() << QString("VideoWidget - reseting pipeline (%0)").arg((long) QThread::currentThreadId());
        dispose();
    }

    if (pipeline) {
        // if pipeline is already created, reset some state and return
        qDebug() << QString("VideoWidget - reseting pipeline state (%0)").arg((long) QThread::currentThreadId());
        lastError = "";
        reset = false;
        return;
    }

    // reset state
    lastError = "";
    reset = false;

    // create pipeline
    qDebug() << QString("VideoWidget - initializing pipeline (%0)").arg((long) QThread::currentThreadId());
    pipeline = createPipelineFromDesc(pipelineDesc.toStdString().c_str(), lastError);
    if (!pipeline) {
        return;
    }
    //gst_pipeline_set_auto_flush_bus(GST_PIPELINE(pipeline), false);

    // register bus synchronous handler
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler) gst_bus_sync_handler, handler, NULL);
    gst_object_unref(bus);
}

void VideoWidget::dispose()
{
    if (!pipeline) {
        return;
    }
    qDebug() << QString("VideoWidget - disposing pipeline (%0)").arg((long) QThread::currentThreadId());

    //GstElement *element = GST_ELEMENT(pipeline);
    //pipeline = NULL;
    gst_pipeline_set_auto_flush_bus(GST_PIPELINE(pipeline), true);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = NULL;

    setOverlay (NULL);

    lastError = "";
    reset = false;

    // TODO should be done async?
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    //	qDebug()
    //			<< QString("paint : %0 - %1 - %2").arg("VideoWidget").arg((long) winId()).arg(
    //					(long) QThread::currentThreadId());
    if (overlay) {
        overlay->expose();
    } else {
        QWidget::paintEvent(event);
        paintStatus(event);
    }
}

void VideoWidget::paintStatus(QPaintEvent *event)
{
    //	qDebug()
    //			<< QString("paint : %0 - %1 - %2").arg("VideoWidget").arg((long) winId()).arg(
    //					(long) QThread::currentThreadId());
    QTextDocument doc;
    doc.setDefaultStyleSheet("* { color:red; }");
//	QString html = "<p>A QTextDocument can be used to present formatted text "
//			"in a nice way.</p>"
//			"<p align=center><b><font size=+2>in</font></b>"
//			"</p>"
//			"<p>The text can be really long and contain many "
//			"paragraphs. It is properly wrapped and such...</p>";
    QString html = "<p align=center><b><font size=+2>" + getStatus() + "</font></b></p>"
            "<p align=center>" + getStatusMessage() + "</p>";

    QRect widgetRect = QWidget::rect();
    int x = 0;
    int w = widgetRect.width();
    int hh = widgetRect.height() / 4;
    int y = (widgetRect.height() - hh) / 2;
    int h = widgetRect.height() - y;
    QRect rect = QRect(x, y, w, h);

    doc.setHtml(html);
    doc.setTextWidth(rect.width());

    QPainter painter(this);
    painter.save();
    painter.translate(rect.topLeft());
    doc.drawContents(&painter, rect.translated(-rect.topLeft()));
    painter.restore();
    //painter.drawRect(rect);

    //		QBrush brush( Qt::yellow );
    //		painter.setBrush( brush );		// set the yellow brush
    //		painter.setPen( Qt::NoPen );		// do not draw outline
    //		painter.drawRect(0, 0, width(), height());	// draw filled rectangle
    //painter.end();

    //QFont font = QApplication::font();
    //font.setPixelSize( rect.height() );
    //painter.setFont( font );
}

QString VideoWidget::getStatus()
{
    if (!lastError.isEmpty()) {
        return "*** ERROR ***";
    } else if (!pipeline && pipelineDesc.isEmpty()) {
        return "*** NO PIPELINE ***";
    }
    return "";
}

QString VideoWidget::getStatusMessage()
{
    if (!lastError.isEmpty()) {
        return lastError;
    }
    return "";
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    //QApplication::syncX();
    //	qDebug()
    //			<< QString("resize : %0 - %1 - %2").arg("VideoWidget").arg((long) winId()).arg(
    //					(long) QThread::currentThreadId());
    if (overlay) {
        overlay->expose();
    } else {
        QWidget::resizeEvent(event);
    }
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
}

QPaintEngine * VideoWidget::paintEngine() const
{
    // bypass double buffering, see setOverlay() for explanation
    return (overlay ? NULL : QWidget::paintEngine());
}

// TODO find a better way and move away from this file
static Pipeline::State cvt(GstState state)
{
    switch (state) {
    case GST_STATE_VOID_PENDING:
        return Pipeline::VoidPending;
    case GST_STATE_NULL:
        return Pipeline::Null;
    case GST_STATE_READY:
        return Pipeline::Ready;
    case GST_STATE_PAUSED:
        return Pipeline::Paused;
    case GST_STATE_PLAYING:
        return Pipeline::Playing;
    }
    return Pipeline::Null;
    //return StateChangedEvent::State.values()[state];
}

// TODO find a better way and move away from this file
static const char * name(Pipeline::State state)
{
    switch (state) {
    case Pipeline::VoidPending:
        return "VoidPending";
    case Pipeline::Null:
        return "Null";
    case Pipeline::Ready:
        return "Ready";
    case Pipeline::Paused:
        return "Paused";
    case Pipeline::Playing:
        return "Playing";
    }
    return "null";
}

bool VideoWidget::event(QEvent *event)
{
    if (event->type() == PipelineEvent::PrepareWindowId) {
        PrepareWindowIdEvent *pe = static_cast<PrepareWindowIdEvent *>(event);
        qDebug() << QString("VideoWidget - got event (%0) : PrepareWindowId").arg((long) QThread::currentThreadId());
        setOverlay(pe->getOverlay());
        return true;
    } else if (event->type() == PipelineEvent::StateChange) {
        StateChangedEvent *sce = static_cast<StateChangedEvent *>(event);
        qDebug()
                << QString("VideoWidget - pipeline %0 changed state from %1 to %2").arg(sce->src).arg(name(sce->getOldState())).arg(
                        name(sce->getNewState()));
        //StateChangedEvent::State oldState = cvt(old_state);
        //Pipeline::State newState = cvt(new_state);
        emit stateChanged(sce->getNewState());
        return true;
    } else if (event->type() == PipelineEvent::Qos) {
        QosEvent *qe = static_cast<QosEvent *>(event);
        qDebug()
                << QString("VideoWidget - element %0 sent qos event:\n%1\n%2\n%3").arg(qe->src).arg(qe->getData().timestamps()).arg(
                        qe->getData().values()).arg(qe->getData().stats());
        return true;
    } else if (event->type() == PipelineEvent::Error) {
        ErrorEvent *ee = static_cast<ErrorEvent *>(event);
        qDebug()
                << QString("VideoWidget - element %0 sent error event:\n%1\n%2").arg(ee->src).arg(ee->getMessage()).arg(
                        ee->getDebug());
        if (lastError.isEmpty()) {
            // remember first error only (usually the most useful)
            lastError = QString("Pipeline error: %0").arg(ee->getMessage());
            // TODO stop pipeline...
            // force reset on next start
            reset = true;
            // need to repaint to display error message
            repaint();
        } else {
            // TODO record subsequent errors separatly
        }
        return true;
    }
    return QWidget::event(event);
}

void VideoWidget::setOverlay(Overlay *overlay)
{
    if (this->overlay != overlay) {
        if (this->overlay) {
            delete this->overlay;
        }
        this->overlay = overlay;
    }

    bool hasOverlay = overlay ? true : false;

    setAutoFillBackground(!hasOverlay);

    // disable background painting to avoid flickering when resizing
    setAttribute(Qt::WA_OpaquePaintEvent, hasOverlay);
    //setAttribute(Qt::WA_NoSystemBackground, hasOverlay); // not sure it is needed

    // disable double buffering to avoid flickering when resizing
    // for this to work we also need to override paintEngine() and make it return NULL.
    // see http://qt-project.org/faq/answer/how_does_qtwa_paintonscreen_relate_to_the_backing_store_widget_composition_
    // drawback is that this widget won't participate in composition...
    setAttribute(Qt::WA_PaintOnScreen, hasOverlay);
}

//static GstElement * createTestPipeline() {
//	GstElement *pipeline = gst_pipeline_new("pipeline");
//	g_assert(pipeline);
//
//	GstElement *src = gst_element_factory_make("videotestsrc", "src");
//	g_assert(src);
//
//	GstElement *sink = gst_element_factory_make("directdrawsink", "sink");
//	g_assert(sink);
//
//	gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
//	gst_element_link_many(src, sink, NULL);
//
//	return pipeline;
//}

static GstElement *createPipelineFromDesc(const char *desc, QString &lastError)
{
    qDebug() << QString("VideoWidget - creating pipeline : %0").arg(desc);
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch_full(desc, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
    if (!pipeline) {
        if (error != NULL) {
            // no pipeline and error...
            // report error to user
            qCritical() << QString("Failed to create pipeline: %0").arg(error->message);
            lastError = QString("Failed to create pipeline: %0").arg(error->message);
            g_error_free(error);
        } else {
            // no pipeline and no error...
            // report generic error
            qCritical() << QString("Failed to create pipeline");
            lastError = QString("Failed to create pipeline");
        }
    } else if (error) {
        // pipeline and error...
        // report error to user?
        // warning?
        qCritical() << QString("Failed to create pipeline: %0").arg(error->message);

        g_error_free(error);
    }
    return pipeline;
}

bool BusSyncHandler::handleMessage(GstMessage *message)
{
    // this method is called by gstreamer as a callback
    // and as such is not necessarily called on the QT event handling thread
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ELEMENT:
        if (gst_is_video_overlay_prepare_window_handle_message(message)) {
            qDebug()
                    << QString("VideoWidget - element %0 prepare window with window #%1").arg(GST_OBJECT_NAME(message->src)).arg(
                            (gulong) wId);
            // prepare-xwindow-id must be handled synchronously in order to have gstreamer use our window
            GstVideoOverlay *gst_video_overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
            gst_video_overlay_set_window_handle(gst_video_overlay, (gulong) wId);
            // and now post event asynchronously
            Overlay *overlay = new GstOverlayImpl(gst_video_overlay);
            QString name = GST_OBJECT_NAME(message->src);
            QCoreApplication::postEvent(widget, new PrepareWindowIdEvent(name, overlay));
        }
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_IS_PIPELINE(message->src)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
            // qDebug() << QString("VideoWidget - element %0 changed state from %1 to %2").arg(GST_OBJECT_NAME(message->src)).arg(gst_element_state_get_name(old_state)).arg(gst_element_state_get_name(new_state));
            QString name = GST_OBJECT_NAME(message->src);
            Pipeline::State oldState = cvt(old_state);
            Pipeline::State newState = cvt(new_state);
            Pipeline::State pendingState = cvt(pending_state);
            QCoreApplication::postEvent(widget, new StateChangedEvent(name, oldState, newState, pendingState));
        }
        break;
    }
    case GST_MESSAGE_QOS: {
        QosData data;
        gboolean live;
        gst_message_parse_qos(message, &live, &data.running_time, &data.stream_time, &data.timestamp, &data.duration);
        data.live = (live == TRUE) ? true : false;
        gdouble proportion;
        gst_message_parse_qos_values(message, &data.jitter, &proportion, &data.quality);
        data.proportion = proportion;
        gst_message_parse_qos_stats(message, NULL, &data.processed, &data.dropped);
        QString name = GST_OBJECT_NAME(message->src);
        QCoreApplication::postEvent(widget, new QosEvent(name, data));
        break;
    }
    case GST_MESSAGE_EOS:
        /* end-of-stream */
        // g_main_loop_quit (loop);
        break;
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;
        gst_message_parse_error(message, &err, &debug);
        QString name = GST_OBJECT_NAME(message->src);
        QCoreApplication::postEvent(widget, new ErrorEvent(name, QString(err->message), QString(debug)));
        g_error_free(err);
        g_free(debug);
        break;
    }
    default:
        break;
    }
    return true;
}

static GstBusSyncReply gst_bus_sync_handler(GstBus *bus, GstMessage *message, BusSyncHandler *handler)
{
    qDebug()
            << QString("VideoWidget - bus_sync_handler (%0) : %1 : %2 (%3)").arg((long) QThread::currentThreadId()).arg(
                    GST_OBJECT_NAME(message->src)).arg(GST_MESSAGE_TYPE_NAME(message)).arg(
                    message != NULL ? GST_OBJECT_NAME(message) : "null");

    if (handler->handleMessage(message)) {
        gst_message_unref(message);
        return GST_BUS_DROP;
    }
    return GST_BUS_PASS;
}
