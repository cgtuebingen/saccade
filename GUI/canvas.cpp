#include <iostream>
#include <cstring>

#include <QMouseEvent>

#include <glog/logging.h>

#include "image_window.h"
#include "canvas.h"
#include "layer.h"
#include "slides.h"
#include "marker.h"
#include "../Utils/gl_manager.h"

bool GUI::Canvas::m_glBlock = false;

// http://blog.qt.io/blog/2014/09/10/qt-weekly-19-qopenglwidget/
GUI::Canvas::Canvas(QWidget *parent, ImageWindow* parentWin)
  : QOpenGLWidget(parent), _parent(parent), _parentWin(parentWin) {

  // specify format
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  setFormat(format);

  // generate context
  _ctx.setFormat(format);
  _ctx.create();
  makeCurrent();

  // handle everthing in custom class using the current context
  _gl = new Utils::GlManager(&_ctx);

  // we allow for dragging, clicking
  setMouseTracking(true);

  // canvas properties
  _property.pixel_size = 1;
  _property.x = 0;
  _property.y = 0;

  _width = 1;
  _height = 1;

  // selection tool
  _selection.rect = QRect(QPoint(0, 0), QSize(0, 0));
  _selection.active = false;

  // no dragging without clicking
  _dragging.active = false;
  _dragging.start.setX(0.0);
  _dragging.start.setY(0.0);

  _slides = new Slides();
  _marker = new Marker();

  emit sigUpdateScrollBars(this);

}

const GUI::Layer* GUI::Canvas::layer(int i) const {
  // if there is any layer return it
  if (!_slides->available())
    return nullptr;
  if (i == -1)
    return _slides->current();
  else
    return _slides->operator[](i);
}

GUI::Layer* GUI::Canvas::layer(int i) {
  // if there is any layer return it
  if (!_slides->available())
    return nullptr;
  if (i == -1)
    return _slides->current();
  else
    return _slides->operator[](i);
}

const GUI::Slides* GUI::Canvas::slides() const {
  // return slide-manager
  // TODO: not yet decided if we really need an advanced std::vector<layer> container
  return _slides;
}

void GUI::Canvas::addLayer(Layer *layer) {
  connect( layer, SIGNAL(sigRefresh()),
           this, SLOT(slotUpdateCanvas()));
  connect( layer, SIGNAL(sigHistogramFinished()),
           this, SLOT(slotUpdateLayer()));
  // canvas gets another image
  _slides->add(layer);
  slotUpdateCanvas();
  slotUpdateLayer();
}

void GUI::Canvas::slotUpdateLayer() {
  emit sigUpdateLayer(this);
  emit sigCoordToImageWindow(canvasToImg(_focus));
}

void GUI::Canvas::slotUpdateCanvas() {
  emit sigUpdateTitle(this);
  emit sigUpdateScrollBars(this);
  emit sigMarkerToImageWindow(*_marker);
  update();
}

void GUI::Canvas::slotSetZoomAction(double zoom) {
  _property.pixel_size = zoom;
  slotUpdateCanvas();
  askSynchronization();
  emit sigPropertyToImagewindow(_property);
}
void GUI::Canvas::slotZoomInAction() {
  // zoom having delta>0
  zoom_rel(_focus, 1);
}

void GUI::Canvas::slotZoomOutAction() {
  // zoom having delta<0
  zoom_rel(_focus, -1);
}

QSize GUI::Canvas::sizeHint() const {
  return QSize(512, 512);
}

void GUI::Canvas::askSynchronization() {
  // something changed here, request synchronization signal to other canvases
  emit sigPropertyChanged(this);
}

void GUI::Canvas::mousePressEvent(QMouseEvent* event) {
  // get position within image coordinates
  QPoint p = canvasToImg(event->pos());

  Qt::KeyboardModifiers keymod = QGuiApplication::keyboardModifiers();

  // left click: enable dragging/shifting
  if ( (event->buttons() & Qt::LeftButton) ) {

    if (keymod == Qt::ShiftModifier) {
      _selection.active = true;
      _selection.rect = QRect(p, QSize(0, 0));
    } else {
      QCursor tmp;
      tmp.setShape( Qt::SizeAllCursor );
      this->setCursor(tmp);

      _dragging.active = true;
      const double ex = event->x();
      const double ey = -event->y();
      _dragging.start.setX(ex / _property.pixel_size - _property.x);
      _dragging.start.setY(ey / _property.pixel_size - _property.y);
    }
  }

  // middle click: set marker
  if ( (event->buttons() & Qt::MidButton) != 0 ) {
    _marker->active = true;
    _marker->x = p.x();
    _marker->y = p.y();
    emit sigMarkerToImageWindow(*_marker);
  }

  // right click: remove marker
  if ( (event->buttons() & Qt::RightButton) != 0) {
    _marker->active = !_marker->active;
    emit sigMarkerToImageWindow(*_marker);
  }

  update();
  emit sigPropertyChanged(this);
}

void GUI::Canvas::mouseMoveEvent(QMouseEvent* event) {
  QPoint p = canvasToImg(event->pos());
  Qt::KeyboardModifiers keymod = QGuiApplication::keyboardModifiers();
  // middle click active --> move marker
  if ( (event->buttons() & Qt::MidButton) ) {
    _marker->active = true;
    _marker->x = p.x();
    _marker->y = p.y();
    update();
    emit sigMarkerToImageWindow(*_marker);
  }

  if ( (event->buttons() & Qt::LeftButton) != 0 && _selection.active) {
    if (keymod == Qt::ShiftModifier) {
      _selection.rect.setBottomRight(p);
    } else {
      _selection.active = false;
    }
    update();
  } else if ( (event->buttons() & Qt::LeftButton) != 0 && _dragging.active) {
    const double dx = event->x();
    const double dy = event->y();
    _property.x = (dx / _property.pixel_size) - _dragging.start.x();
    _property.y = -(dy / _property.pixel_size) - _dragging.start.y();
    update();
    emit sigPropertyChanged(this);
    emit sigUpdateScrollBars(this);
  }
  emit sigCoordToImageWindow(p);

  _focus = event->pos();
}

/**
 * @brief move canvas such that pixel is in the center
 * @details [long description]
 *
 * @param imgCoord coordinate in image (w, h)
 * @return [description]
 */
void GUI::Canvas::focusOnPixel(QPoint imgCoord) {

  // offset in image to the right
  const double left_space = imgCoord.x();
  const double top_space = imgCoord.y();

  //
  const double scaled_img_width = _slides->width()  * _property.pixel_size;
  const double scaled_img_height = _slides->height()  * _property.pixel_size;


  const double percent_width = imgCoord.x() / scaled_img_width;
  const double percent_height = imgCoord.y() / scaled_img_height;


  _property.x = percent_width * scaled_img_width + scaled_img_width / 2;
  _property.y = scaled_img_height * -scaled_img_height + scaled_img_height / 2;

}


void GUI::Canvas::mouseReleaseEvent(QMouseEvent* event) {
  QPoint p = canvasToImg(event->pos());

  if (_selection.active) {
    _selection.active = false;
    _selection.rect.setBottomRight(p);

    LOG(INFO) << "selection "
              << _selection.rect.center().y() << " "
              << _selection.rect.center().x() << " ";

    const double nth_pixel_hor = _selection.rect.center().x();
    const double nth_pixel_ver = _selection.rect.center().y();

    const double visible_pixel_hor = width() / _property.pixel_size;
    const double visible_pixel_ver = height() / _property.pixel_size;

    const double hor_percent_offset = nth_pixel_hor / (double)_slides->width();
    const double ver_percent_offset = nth_pixel_ver / (double)_slides->height();

    // TODO !!!

    slotUpdateCanvas();
    emit sigPropertyChanged(this);
    emit sigPropertyToImagewindow(_property);

  } else if ( _dragging.active ) {
    QCursor tmp;
    tmp.setShape( Qt::ArrowCursor );
    this->setCursor(tmp);
    _dragging.active = false;
  }
}


void GUI::Canvas::wheelEvent( QWheelEvent * event) {
  if (_slides->available() > 0) {
    zoom_rel(event->pos(), event->delta());
  }

}

void GUI::Canvas::slotFitZoomToWindow() {
  LOG(INFO) << "GUI::Canvas::slotFitZoomToWindow";
  const double zoom_width = ((double)_width / (double)_slides->width());
  const double zoom_height = ((double)_height / (double)_slides->height());
  _property.pixel_size = std::min(zoom_width, zoom_height);

  _property.x = 0;
  _property.y = 0;

  slotUpdateCanvas();
  emit sigPropertyChanged(this);
  emit sigPropertyToImagewindow(_property);
}

void GUI::Canvas::slotCenterImage() {
  _property.x = 0;
  _property.y = 0;

  slotUpdateCanvas();
  emit sigPropertyChanged(this);
  emit sigPropertyToImagewindow(_property);
}

void GUI::Canvas::slotFitToImage() {
  const int img_width = _slides->width() * _property.pixel_size;
  const int img_height = _slides->height() * _property.pixel_size;

  const int padding_w = (_parentWin->width() - width());
  const int padding_h = (_parentWin->height() - height());

  _parentWin->resize(img_width + padding_w + 1, img_height + padding_h + 1);

  _property.x = 0;
  _property.y = 0;

  slotUpdateCanvas();
  emit sigPropertyChanged(this);
  emit sigPropertyToImagewindow(_property);
}

void GUI::Canvas::zoom_rel(QPoint q, int delta) {

  const double zoom_delta = sqrt(2.0);
  const double old_zoom =  _property.pixel_size;

  double new_zoom = 0;
  if ( delta > 0 )
    new_zoom = _property.pixel_size * zoom_delta;
  else
    new_zoom = _property.pixel_size / zoom_delta;

  const unsigned int winWidth = width();
  const unsigned int winHeight = height();
  double mpercX = ((q.x()) / ((double)winWidth)) - 0.5;
  double mpercY = ((q.y()) / ((double)winHeight)) - 0.5;

  // change zoom value
  if (new_zoom <= 0.)
    new_zoom = 1.;

  QPoint q_img = canvasToImg(q);
  if ( (q_img.x() < 0) ||
       (q_img.y() < 0) ||
       (q_img.x() >= (int)_slides->width()) ||
       (q_img.y() >= (int)_slides->height())) {
    _property.x = 0;
    _property.y = 0;
    _property.pixel_size = new_zoom;
  } else {
    _property.x += ((double)winWidth * ((1.0 / new_zoom) - (1.0 / old_zoom)) * mpercX);
    _property.y -= ((double)winHeight * ((1.0 / new_zoom) - (1.0 / old_zoom)) * mpercY);
    _property.pixel_size = new_zoom;
  }

  slotUpdateCanvas();

  emit sigPropertyChanged(this);
  emit sigPropertyToImagewindow(_property);

}

void GUI::Canvas::resizeGL(int width, int height) {
  _width = width;
  _height = height;

  _gl->set_size(width, height);
  _gl->projection_identity();
  _gl->set_ortho(width, height);
  _gl->modelview_identity();
}

void GUI::Canvas::checkerboard(unsigned char* data,
                               unsigned int width,
                               unsigned int height,
                               unsigned int channels) {

  memset(data, 0, width * height * channels);

  for (unsigned int h = 0; h < height; h++ ) {
    const int off = h % 2;
    for (unsigned int w = 0; w < width; w += 2 ) {
      for (unsigned int c = 0; c < channels; c++ ) {
        data[(h * width + w + off)*channels + c] = 255;
      }
    }
  }
}

/**
 * @brief from global canvas position to coordinate within image
 * @details [long description]
 *
 * @param p [description]
 * @return [description]
 */
QPoint GUI::Canvas::canvasToImg( QPoint p ) const {
  if (!_slides->available())
    return QPoint(0, 0);

  const double pixel_size = _property.pixel_size;
  const double image_width = _slides->width();
  const double image_height = _slides->height();
  const double canvas_width = width() - 1.0;
  const double canvas_height = height() - 1.0;

  const double padding_w = 0.5 * (image_width * pixel_size - canvas_width);
  const double padding_h = 0.5 * (image_height * pixel_size - canvas_height);

  double px = p.x();
  double py = p.y() - 1.0;

  // with rotation
  // const double alpha = _property.angle * -(M_PI / 180.0);
  // double x = (px + padding_w - _property.x * pixel_size) - 0.5 * image_width * pixel_size;
  // double y = (py + padding_h + _property.y * pixel_size) - 0.5 * image_height * pixel_size;
  // px = (x * cos(alpha) - y * sin(alpha)) / pixel_size + 0.5 * image_width;
  // py = (y * cos(alpha) + x * sin(alpha)) / pixel_size + 0.5 * image_height;

  px = (px + padding_w)  / pixel_size - _property.x;
  py = (py + padding_h)  / pixel_size + _property.y;

  return QPoint( (int)px, (int)py);
}

QPoint GUI::Canvas::imgToCanvas( QPoint p ) const {
  if (!_slides->available())
    return QPoint(0, 0);

  const double pixel_size = _property.pixel_size;
  const double image_width = _slides->width();
  const double image_height = _slides->height();
  const double canvas_width = width() - 1.0;
  const double canvas_height = height() - 1.0;

  const double padding_w = 0.5 * (image_width * pixel_size - canvas_width);
  const double padding_h = 0.5 * (image_height * pixel_size - canvas_height);

  double px = p.x();
  double py = p.y();

  px = (px + _property.x) * pixel_size - padding_w;
  py = (py - _property.y) * pixel_size - padding_h + 1;

  return QPoint( (int)px, (int)py);
}


GUI::Canvas::property_t GUI::Canvas::getProperty() {
  return _property;
}

void GUI::Canvas::setProperty( property_t property ) {
  if (_slides->available()) {
    _property = property;
    update();
  }
}

GUI::Marker GUI::Canvas::getMarker() {
  return *_marker;
}

void GUI::Canvas::setMarker( GUI::Marker marker ) {
  *_marker = marker;
  update();
}

void GUI::Canvas::updatePropertyByScrollbar( property_t property ) {

  _property.x = property.x;
  _property.y = property.y;
  emit sigPropertyChanged(this);
  update();
}

void GUI::Canvas::initializeGL() {
  LOG(INFO) << "GUI::Canvas::initializeGL()";
  _gl->initializeOpenGLFunctions();
  _gl->printContextInformation();

  _gl->glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
  _gl->set_size(width(), height());
  _gl->projection_identity();
  _gl->modelview_identity();
  _gl->enable_texture_blend();

  // generate checkerboard texture
  _bg = new Utils::GlObject<unsigned char>(512, 512, 4);
  _bg->allocate();
  checkerboard(_bg->data, _bg->width, _bg->height, _bg->channels);

  _gl->prepare(_bg);
  _gl->draw(_bg, -4000, 4000, 4000, -4000, 0);


  delete[] _bg->data;
}


void GUI::Canvas::paintGL() {
  while ( !__sync_bool_compare_and_swap (&m_glBlock, false, true));

  _gl->identity();
  _gl->clear();

  // draw background
  _gl->draw(_bg, -4000, 4000, 4000, -4000, 0);

  if (_slides->available()) {
    // get corner points
    QPoint top_left = QPoint(0, 0);
    QPoint top_right = QPoint(width() - 1, 0);
    QPoint bottom_left = QPoint(0, height() - 1);
    QPoint bottom_right = QPoint(width() - 1, height() - 1);
    // map to image
    top_left = canvasToImg(top_left);
    top_right = canvasToImg(top_right);
    bottom_left = canvasToImg(bottom_left);
    bottom_right = canvasToImg(bottom_right);

    int left, right, top, bottom;
    left = std::min( top_left.x(), std::min( top_right.x(), std::min( bottom_left.x(), bottom_right.x() )));
    top = std::min( top_left.y(), std::min( top_right.y(), std::min( bottom_left.y(), bottom_right.y() )));
    right = std::max( top_left.x(), std::max( top_right.x(), std::max( bottom_left.x(), bottom_right.x() )));
    bottom = std::max( top_left.y(), std::max( top_right.y(), std::max( bottom_left.y(), bottom_right.y() )));


    glColor3d( 1.0, 0.0, 0.0 );
    glScaled(_property.pixel_size, -_property.pixel_size, 1.0);
    glTranslated(_property.x, -_property.y, 0.0);
    glRotatef(0, 0, 0, 1.0);


    const double image_width = _slides->width();
    const double image_height = _slides->height();
    glTranslated(-0.5 * image_width,
                 -0.5 * image_height, 0.0);

    _slides->draw(_gl, top, left,
                  bottom, right,
                  _property.pixel_size);

    _gl->drawMarker(this, _marker);
    if (_selection.active) {
      _gl->drawSelection(this, _selection.rect);
    }
  }


  m_glBlock = false;
}


void GUI::Canvas::slotPrevLayer() {
  _slides->backward();
  slotUpdateLayer();
  slotUpdateCanvas();
}
void GUI::Canvas::slotRemoveCurrentLayer() {
  if (_slides->available()) {
    _slides->remove();
    slotUpdateCanvas();
  }
  LOG(INFO) << "remove layer";
}
void GUI::Canvas::slotNextLayer() {
  _slides->forward();
  slotUpdateLayer();
  slotUpdateCanvas();
}


/*
QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
*/