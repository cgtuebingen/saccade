#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "../Utils/gl_object.h"
#include "marker.h"

namespace Utils {
class GlManager;
}; // namespace Utils

namespace GUI {
class ImageWindow;
class Slides;
class Layer;
// class Marker;

class Canvas  : public QOpenGLWidget {
  Q_OBJECT
 public:
  struct property_t {
    double zoom_factor = 1.;
    // not supported yet (should be rotation)
    double angle = 0;
    // center of image relative to canvas (0, 0) means centered
    double x, y;
  };
 private:
  QWidget* _parent;
  ImageWindow* _parentWin;

  QOpenGLContext _ctx;

  Utils::GlObject<unsigned char> *_bg;

  struct drag_t {
    QPointF start;
    bool active;
  } _dragging;

  property_t _property;

  int _width, _height;

  Slides* _slides;
  Utils::GlManager *_gl;

  Marker* _marker;

  QPoint _focus;

 public:

  Canvas(QWidget *parent, ImageWindow* parentWin);
  QSize sizeHint() const;

  void initializeGL();
  void resizeGL(int w, int h);
  void paintGL();

  void addLayer(Layer* layer);

  const Layer* layer(int i = -1) const;
  Layer* layer(int i = -1);
  const Slides* slides() const;

  void askSynchronization();

  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);
  void wheelEvent( QWheelEvent * event);

  QPoint screenToBuf( QPoint p ) const;

  void setProperty( property_t property );
  property_t getProperty();

  void setMarker( Marker marker );
  Marker getMarker();

  void updatePropertyByScrollbar( property_t property );

 signals:
  void sigPropertyChanged(Canvas*);
  void sigUpdateTitle(Canvas*);
  void sigUpdateLayer(Canvas*);
  void sigUpdateScrollBars(Canvas*);

  void sigCoordToImageWindow(QPoint);
  void sigMarkerToImageWindow(Marker);
  void sigPropertyToImagewindow(Canvas::property_t);

 public slots:
  void slotPrevLayer();
  void slotNextLayer();
  void slotRemoveCurrentLayer();

  void slotUpdateLayer();
  void slotUpdateCanvas();

  void slotZoomInAction();
  void slotZoomOutAction();

  void slotSetZoomAction(double);
  void slotFitZoomToWindow();
  void slotFitToImage();
  void slotCenterImage();

 protected:
  void zoom(QPoint q, int delta);
  void checkerboard(unsigned char* data,
                    unsigned int width = 512,
                    unsigned int height = 512,
                    unsigned int channels = 4);
 private slots:

 public:
  static bool m_glBlock;
};
}; // namespace GUI

#endif // BUFFER_H