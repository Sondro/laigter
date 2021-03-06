/*
 * Laigter: an automatic map generator for lighting effects.
 * Copyright (C) 2019  Pablo Ivan Fonovich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * Contact: azagaya.games@gmail.com
 */

#include "imageprocessor.h"
#include <QApplication>
#include <QDebug>
#include <cmath>
#include <QtConcurrent/QtConcurrent>

using namespace cv;

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent) {
  position = offset = QVector2D(0, 0);
  zoom = 1.0;
  selected = false;

  normal_bisel_depth = 1000;
  normal_bisel_distance = 60;
  normal_depth = 100;
  normal_blur_radius = 5;
  normal_bisel_blur_radius = 10;
  gradient_end = 1;
  normal_bisel_soft = true;
  normalInvertX = normalInvertY = normalInvertZ = 1;
  tileable = false;
  tileX = false;
  tileY = false;
  busy = false;

  parallax_max = 140;
  parallax_min = 0;
  parallax_invert = false;
  parallax_focus = 3;
  parallax_soft = 10;
  parallax_quantization = 1;
  parallax_type = ParallaxType::Binary;
  parallax_brightness = 0;
  parallax_contrast = 1;
  parallax_erode_dilate = 1;

  specular_blur = 10;
  specular_bright = 0;
  specular_contrast = 1;
  specular_thresh = 127;
  specular_invert = false;
  specular_base_color = Vec4b(0, 0, 0, 0);

  occlusion_blur = 10;
  occlusion_bright = 16;
  occlusion_contrast = 1;
  occlusion_thresh = 1;
  occlusion_invert = false;
  occlusion_distance_mode = true;
  occlusion_distance = 10;

  settings.tileable = &tileable;
  settings.gradient_end = &gradient_end;
  settings.parallax_max = &parallax_max;
  settings.parallax_min = &parallax_min;
  settings.parallax_soft = &parallax_soft;
  settings.parallax_type = &parallax_type;
  settings.parallax_focus = &parallax_focus;
  settings.parallax_invert = &parallax_invert;
  settings.parallax_contrast = &parallax_contrast;
  settings.parallax_brightness = &parallax_brightness;
  settings.parallax_erode_dilate = &parallax_erode_dilate;
  settings.parallax_quantization = &parallax_quantization;

  settings.normal_depth = &normal_depth;
  settings.normalInvertX = &normalInvertX;
  settings.normalInvertY = &normalInvertY;
  settings.normalInvertZ = &normalInvertZ;
  settings.normal_bisel_soft = &normal_bisel_soft;
  settings.normal_bisel_depth = &normal_bisel_depth;
  settings.normal_blur_radius = &normal_blur_radius;
  settings.normal_bisel_distance = &normal_bisel_distance;
  settings.normal_bisel_blur_radius = &normal_bisel_blur_radius;

  settings.specular_blur = &specular_blur;
  settings.specular_bright = &specular_bright;
  settings.specular_invert = &specular_invert;
  settings.specular_thresh = &specular_thresh;
  settings.specular_contrast = &specular_contrast;

  settings.occlusion_blur = &occlusion_blur;
  settings.occlusion_bright = &occlusion_bright;
  settings.occlusion_invert = &occlusion_invert;
  settings.occlusion_thresh = &occlusion_thresh;
  settings.occlusion_contrast = &occlusion_contrast;
  settings.occlusion_distance = &occlusion_distance;
  settings.occlusion_distance_mode = &occlusion_distance_mode;

  settings.lightList = &lightList;

  is_parallax = false;
  connected = false;

  customSpecularMap = false;
  customHeightMap = false;

  active = true;

  normal_counter = parallax_counter = specular_counter = occlussion_counter = 0;

}

ImageProcessor::~ImageProcessor(){
  active = false;
  while(normal_counter > 0){
    QThread::msleep(10);
  }
}

int ImageProcessor::loadImage(QString fileName, QImage image) {
  m_fileName = fileName;
  m_name = fileName;
  texture = image;

  normalOverlay = QImage(image.width(),image.height(),QImage::Format_RGBA8888_Premultiplied);
  normalOverlay.fill(QColor(0,0,0,0));

  specularOverlay = QImage(image.width(),image.height(),QImage::Format_RGBA8888_Premultiplied);
  specularOverlay.fill(QColor(0,0,0,0));

  heightOverlay = QImage(image.width(),image.height(),QImage::Format_RGBA8888_Premultiplied);
  heightOverlay.fill(QColor(128,128,128,0));

  parallaxOverlay = QImage(image.width(),image.height(),QImage::Format_RGBA8888_Premultiplied);
  parallaxOverlay.fill(QColor(0,0,0,0));

  occlussionOverlay = QImage(image.width(), image.height(), QImage::Format_RGBA8888_Premultiplied);
  occlussionOverlay.fill(QColor(0,0,0,0));

  m_img = Mat(image.height(), image.width(), CV_8UC4, image.scanLine(0));
  int aux = m_img.depth();
  switch (aux) {
  case CV_8S:
    m_img.convertTo(m_img, CV_8U, 0, 128);
    break;
  case CV_16U:
    m_img.convertTo(m_img, CV_8U, 1 / 255.0);
    break;
  case CV_16S:
    m_img.convertTo(m_img, CV_8U, 1 / 255.0, 128);
    break;
  case CV_32S:
    m_img.convertTo(m_img, CV_8U, 1 / 255.0 / 255.0, 128);
    break;
  case CV_32F:
  case CV_64F:
    m_img.convertTo(m_img, CV_8U, 255);
    break;
  }

  if (m_img.channels() < 4) {
    if (m_img.channels() == 3) {
      cvtColor(m_img, m_img, COLOR_RGB2RGBA);
    } else {
      cvtColor(m_img, m_img, COLOR_GRAY2RGBA);
    }
  }
  if (!customSpecularMap) {
    m_img.copyTo(m_specular);
  }
  if (!customHeightMap) {
    neighbours = Mat::zeros(m_img.rows * 3, m_img.cols * 3, m_img.type());

    m_img.copyTo(m_heightmap);
    m_img.copyTo(m_occlusion);

    m_height_ov = Mat(m_img.rows, m_img.cols,CV_32FC3);
    aux_height_ov = Mat(m_img.rows, m_img.cols,CV_32FC1);
    aux_height_ov = Scalar::all(0.0);
    m_emboss_normal = Mat(m_img.rows, m_img.cols,CV_32FC3);
    m_distance_normal = Mat(m_img.rows, m_img.cols,CV_32FC3);

    fill_neighbours(m_heightmap, neighbours);
  }

  //  m_normal = Mat(m_img.rows,m_img.cols,CV_8UC3);
  return 0;
}

void ImageProcessor::set_current_heightmap() {
  current_heightmap = tileable ? neighbours : m_heightmap;
  cvtColor(current_heightmap, m_parallax, CV_RGBA2GRAY);
  cvtColor(current_heightmap, m_occlusion, COLOR_RGBA2GRAY, 1);
}

void ImageProcessor::calculate() {

  set_current_heightmap();
  calculate_distance();

  calculate_heightmap();
  generate_normal_map();
  calculate_parallax();
  calculate_specular();
  calculate_occlusion();
}

void ImageProcessor::calculate_parallax() {
  if (parallax_counter > 2)
    return;
  parallax_counter++;
  QMutexLocker locker(&parallax_mutex);

  QImage ovi = get_parallax_overlay();
  Mat ov = Mat(ovi.height(), ovi.width(), CV_8UC4, ovi.scanLine(0));

  Mat channels[4];
  split(ov, channels);

  Mat alpha = channels[3];
  ov = channels[0];
  alpha.convertTo(alpha, CV_32FC1, 1.0/255);
  ov.convertTo(ov, CV_32FC1, 1.0/255);

  current_parallax = modify_parallax();

  Rect rect(m_img.cols, m_img.rows, m_img.cols, m_img.rows);
  if (tileable && current_parallax.rows == m_img.rows * 3) {
    current_parallax(rect).copyTo(current_parallax);
  }

  switch (current_parallax.channels()) {
  case 3:
    cvtColor(current_parallax, current_parallax, COLOR_RGB2GRAY);
    break;
  case 4:
    cvtColor(current_parallax, current_parallax, COLOR_RGBA2GRAY);
    break;
  }

  current_parallax.convertTo(current_parallax, CV_32FC1, 1.0/255);
  multiply(Scalar::all(1.0)-alpha,current_parallax,current_parallax);
  add(ov, current_parallax, current_parallax);
  current_parallax.convertTo(current_parallax, CV_8UC1, 255);


  parallax_ready.lock();
  parallax = QImage(static_cast<unsigned char *>(current_parallax.data),
                    current_parallax.cols, current_parallax.rows,
                    current_parallax.step, QImage::Format_Grayscale8);
  parallax_ready.unlock();
  processed();
  parallax_counter--;
}

void ImageProcessor::calculate_specular() {
  if (specular_counter > 2)
    return;
  specular_counter++;
  QMutexLocker locker(&specular_mutex);
  current_specular = modify_specular();

  QImage ovi = get_specular_overlay();
  Mat ov = Mat(ovi.height(), ovi.width(), CV_8UC4, ovi.scanLine(0));
  Mat channels[4];
  split(ov, channels);

  Mat alpha = channels[3];
  ov = channels[0];
  alpha.convertTo(alpha, CV_32FC1, 1.0/255);
  ov.convertTo(ov, CV_32FC1, 1.0/255);

  // multiply(alpha,ov,ov);

  Rect rect(m_img.cols, m_img.rows, m_img.cols, m_img.rows);
  if (tileable && current_specular.rows == m_img.rows * 3) {
    current_specular(rect).copyTo(current_specular);
  }



  switch (current_specular.channels()) {
  case 3:
    cvtColor(current_specular, current_specular, COLOR_RGB2GRAY);
    break;
  case 4:
    cvtColor(current_specular, current_specular, COLOR_RGBA2GRAY);
    break;
  }

  current_specular.convertTo(current_specular, CV_32FC1, 1.0/255);
  multiply(Scalar::all(1.0)-alpha,current_specular,current_specular);
  add(ov, current_specular, current_specular);
  current_specular.convertTo(current_specular, CV_8UC1, 255);

  specular_ready.lock();
  specular = QImage(static_cast<unsigned char *>(current_specular.data),
                    current_specular.cols, current_specular.rows,
                    current_specular.step, QImage::Format_Grayscale8);
  specular_ready.unlock();

  specular_counter--;

  processed();
}

void ImageProcessor::calculate_occlusion() {
  if (occlussion_counter > 2)
    return;
  occlussion_counter++;
  QMutexLocker locker(&occlusion_mutex);
  current_occlusion = modify_occlusion();

  QImage ovi = get_occlusion_overlay();
  Mat ov = Mat(ovi.height(), ovi.width(), CV_8UC4, ovi.scanLine(0));
  Mat channels[4];
  split(ov, channels);

  Mat alpha = channels[3];
  ov = channels[0];
  alpha.convertTo(alpha, CV_32FC1, 1.0/255);
  ov.convertTo(ov, CV_32FC1, 1.0/255);

  Rect rect(m_img.cols, m_img.rows, m_img.cols, m_img.rows);
  if (tileable && current_occlusion.rows == m_img.rows * 3) {
    current_occlusion(rect).copyTo(current_occlusion);
  }

  switch (current_occlusion.channels()) {
  case 3:
    cvtColor(current_occlusion, current_occlusion, COLOR_RGB2GRAY);
    break;
  case 4:
    cvtColor(current_occlusion, current_occlusion, COLOR_RGBA2GRAY);
    break;
  }

  current_occlusion.convertTo(current_occlusion, CV_32FC1, 1.0/255);
  multiply(Scalar::all(1.0)-alpha,current_occlusion,current_occlusion);
  add(ov, current_occlusion, current_occlusion);
  current_occlusion.convertTo(current_occlusion, CV_8UC1, 255);

  occlussion_ready.lock();
  occlussion = QImage(static_cast<unsigned char *>(current_occlusion.data),
                      current_occlusion.cols, current_occlusion.rows,
                      current_occlusion.step, QImage::Format_Grayscale8);
  occlussion_ready.unlock();
  processed();

  occlussion_counter--;
}

void ImageProcessor::calculate_heightmap() {
  cvtColor(current_heightmap, m_gray, COLOR_RGBA2GRAY);
  if (m_gray.type() != CV_32FC1)
    m_gray.convertTo(m_gray, CV_32FC1);
}

int ImageProcessor::fill_neighbours(Mat src, Mat dst) {
  if (src.cols != dst.cols / 3 || src.rows != dst.rows / 3) {
    return -1;
  }
  Rect rect;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      rect = Rect(i * src.cols, j * src.rows, src.cols, src.rows);
      src.copyTo(dst(rect));
    }
  }
  calculate();
  return 0;
}

void ImageProcessor::reset_neighbours() {

  fill_neighbours(m_heightmap, neighbours);
}

int ImageProcessor::empty_neighbour(int x, int y) {
  Mat n = Mat::zeros(m_heightmap.rows, m_heightmap.cols, m_heightmap.type());
  set_neighbour(n, neighbours, x, y);
  return 0;
}

int ImageProcessor::set_neighbour(Mat src, Mat dst, int x, int y) {
  if (src.cols != dst.cols / 3 || src.rows != dst.rows / 3) {
    return -1;
  }
  Rect rect(y * src.cols, x * src.rows, src.cols, src.rows);
  src.copyTo(dst(rect));
  return 0;
}

int ImageProcessor::set_neighbour_image(QString fileName, QImage image, int x,
                                        int y) {

  Mat n = Mat(image.height(), image.width(), CV_8UC4, image.scanLine(0));

  int aux = n.depth();
  switch (aux) {
  case CV_8S:
    n.convertTo(n, CV_8U, 0, 128);
    break;
  case CV_16U:
    n.convertTo(n, CV_8U, 1 / 255.0);
    break;
  case CV_16S:
    n.convertTo(n, CV_8U, 1 / 255.0, 128);
    break;
  case CV_32S:
    n.convertTo(n, CV_8U, 1 / 255.0 / 255.0, 128);
    break;
  case CV_32F:
  case CV_64F:
    n.convertTo(n, CV_8U, 255);
    break;
  }

  if (n.channels() < 4) {
    if (n.channels() == 3) {
      cvtColor(n, n, COLOR_RGB2RGBA);
    } else {
      cvtColor(n, n, COLOR_GRAY2RGBA);
    }
  }
  // cvtColor(n,n,COLOR_RGBA2BGRA);
  cv::resize(n, n, m_img.size() * 2);
  cv::resize(n, n, m_img.size());

  set_neighbour(n, neighbours, x, y);

  return 0;
}

QImage ImageProcessor::get_neighbour(int x, int y) {
  Rect rect(y * m_img.cols, x * m_img.rows, m_img.cols, m_img.rows);
  neighbours(rect).copyTo(m_aux);
  // cvtColor(m_aux,m_aux,CV_BGRA2RGBA);
  QImage p =
    QImage(static_cast<unsigned char *>(m_aux.data), m_aux.cols, m_aux.rows,
           m_aux.step, QImage::Format_RGBA8888_Premultiplied);
  return p;
}

QString ImageProcessor::get_specular_path() { return m_specularPath; }

QString ImageProcessor::get_heightmap_path() { return m_heightmapPath; }

int ImageProcessor::loadSpecularMap(QString fileName, QImage specular) {
  if (fileName == get_name()) {
    m_specularPath = "";
    customSpecularMap = false;
  } else {
    m_specularPath = fileName;
  }
  customSpecularMap = true;
  m_specular =
    Mat(specular.height(), specular.width(), CV_8UC4, specular.scanLine(0));

  int aux = m_specular.depth();
  switch (aux) {
  case CV_8S:
    m_specular.convertTo(m_specular, CV_8U, 0, 128);
    break;
  case CV_16U:
    m_specular.convertTo(m_specular, CV_8U, 1 / 255.0);
    break;
  case CV_16S:
    m_specular.convertTo(m_specular, CV_8U, 1 / 255.0, 128);
    break;
  case CV_32S:
    m_specular.convertTo(m_specular, CV_8U, 1 / 255.0 / 255.0, 128);
    break;
  case CV_32F:
  case CV_64F:
    m_specular.convertTo(m_specular, CV_8U, 255);
    break;
  }

  if (m_specular.channels() < 4) {
    if (m_specular.channels() == 3) {
      cvtColor(m_specular, m_specular, COLOR_RGB2RGBA);
    } else {
      cvtColor(m_specular, m_specular, COLOR_GRAY2RGBA);
    }
  }
  // cvtColor(m_specular,m_specular,COLOR_RGBA2BGRA);
  cv::resize(m_specular, m_specular, m_img.size() * 2);
  cv::resize(m_specular, m_specular, m_img.size());

  calculate();

  return 0;
}

int ImageProcessor::loadHeightMap(QString fileName, QImage height) {
  if (fileName == get_name()) {
    m_heightmapPath = "";
    customHeightMap = false;
  } else {
    m_heightmapPath = fileName;
  }
  customHeightMap = true;
  m_heightmap =
    Mat(height.height(), height.width(), CV_8UC4, height.scanLine(0));

  int aux = m_heightmap.depth();
  switch (aux) {
  case CV_8S:
    m_heightmap.convertTo(m_heightmap, CV_8U, 0, 128);
    break;
  case CV_16U:
    m_heightmap.convertTo(m_heightmap, CV_8U, 1 / 255.0);
    break;
  case CV_16S:
    m_heightmap.convertTo(m_heightmap, CV_8U, 1 / 255.0, 128);
    break;
  case CV_32S:
    m_heightmap.convertTo(m_heightmap, CV_8U, 1 / 255.0 / 255.0, 128);
    break;
  case CV_32F:
  case CV_64F:
    m_heightmap.convertTo(m_heightmap, CV_8U, 255);
    break;
  }

  if (m_heightmap.channels() < 4) {
    if (m_heightmap.channels() == 3) {
      cvtColor(m_heightmap, m_heightmap, COLOR_RGB2RGBA);
    } else {
      cvtColor(m_heightmap, m_heightmap, COLOR_GRAY2RGBA);
    }
  }
  // cvtColor(m_heightmap,m_heightmap,COLOR_RGBA2BGRA);
  cv::resize(m_heightmap, m_heightmap, m_img.size() * 2);
  cv::resize(m_heightmap, m_heightmap, m_img.size());

  cvtColor(m_heightmap, m_gray, COLOR_RGBA2GRAY);

  set_neighbour(m_heightmap, neighbours, 1, 1);

  if (m_gray.type() != CV_32FC1)
    m_gray.convertTo(m_gray, CV_32FC1);

  calculate();

  return 0;
}

void ImageProcessor::set_name(QString name) { m_name = name; }

QString ImageProcessor::get_name() { return m_name; }
void ImageProcessor::calculate_gradient() {}

void ImageProcessor::calculate_distance() {

  if (!current_heightmap.ptr<int>(0))
    return;

  cvtColor(current_heightmap, m_distance, COLOR_RGBA2GRAY, 1);

  for (int x = 0; x < m_distance.rows; ++x) {
    uchar *pixel = m_distance.ptr<uchar>(x);
    for (int y = 0; y < m_distance.cols; ++y) {
      if (!tileable && (x == 0 || y == 0 || x == m_distance.rows - 1 ||
                        y == m_distance.cols - 1)) {
        // m_distance.at<unsigned char>(x,y) = 0;
        *pixel = 0;
      } else if (current_heightmap.at<Vec4b>(x, y)[3] == 0.0) {
        // m_distance.at<unsigned char>(x,y) = 0;
        *pixel = 0;
      } else {
        m_distance.at<unsigned char>(x, y) = 255;
        *pixel = 255;
      }
      pixel++;
    }
  }
  threshold(m_distance, m_distance, 0, 255, THRESH_BINARY);

  distanceTransform(m_distance, m_distance, CV_DIST_L2, 5);
  m_distance.convertTo(m_distance, CV_32FC1, 1.0 / 255);

  m_distance.copyTo(new_distance);
  //  new_distance = modify_distance();
}

void ImageProcessor::set_normal_invert_x(bool invert) {
  normalInvertX = -invert * 2 + 1;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,true,false,QRect(0,0,0,0));
}
void ImageProcessor::set_normal_invert_y(bool invert) {
  normalInvertY = -invert * 2 + 1;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,true,false,QRect(0,0,0,0));
}
void ImageProcessor::set_normal_invert_z(bool invert) {
  normalInvertZ = -invert * 2 + 1;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,true,false,QRect(0,0,0,0));
}
void ImageProcessor::set_normal_depth(int depth) {
  normal_depth = depth;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,false,false,QRect(0,0,0,0));
}
void ImageProcessor::set_normal_bisel_soft(bool soft) {
  normal_bisel_soft = soft;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,false,true,true,QRect(0,0,0,0));
}
void ImageProcessor::set_normal_blur_radius(int radius) {
  normal_blur_radius = radius;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,false,false,QRect(0,0,0,0));
}

void ImageProcessor::set_normal_bisel_depth(int depth) {
  normal_bisel_depth = depth;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,false,true,false,QRect(0,0,0,0));
}

void ImageProcessor::set_normal_bisel_distance(int distance) {
  normal_bisel_distance = distance;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,false,true,true,QRect(0,0,0,0));
}

void ImageProcessor::set_tileable(bool t) {
  tileable = t;
  update_tileable = true;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,true,true,true,QRect(0,0,0,0));
}

bool ImageProcessor::get_tileable() { return tileable; }

Mat ImageProcessor::modify_distance() {
  Mat m;
  m_distance.copyTo(m);
  for (int x = 0; x < m.rows; ++x) {
    float *pixel = m.ptr<float>(x);
    for (int y = 0; y < m.cols; ++y) {
      if (normal_bisel_distance == 0) {
        *pixel = 0;
      } else {
        *pixel *= 255.0 / normal_bisel_distance;
        if (*pixel > 1)
          *pixel = 1;
        if (normal_bisel_soft) {
          double d = *pixel;
          *pixel = sqrt(1 - pow((d - 1), 2));
        }
      }
      pixel++;
    }
  }
  return m;
}

Mat ImageProcessor::modify_occlusion() {
  Mat m;

  m_occlusion.copyTo(m);
  if (occlusion_invert) {
    subtract(Scalar::all(255), m, m);
  }
  if (occlusion_distance_mode) {
    threshold(m, m, occlusion_thresh, 255, THRESH_BINARY);
    distanceTransform(m, m, CV_DIST_L2, 5);
    m.convertTo(m, CV_32F, 1 / 255.0);
    for (int x = 0; x < m.rows; ++x) {
      float *pixel = m.ptr<float>(x);
      for (int y = 0; y < m.cols; ++y) {
        if (occlusion_distance == 0) {
          *pixel = 1.0;
        } else {
          *pixel *= 255.0 / occlusion_distance;
          if (*pixel > 1)
            *pixel = 1;
          double d = *pixel;
          *pixel = sqrt(1 - pow((d - 1), 2));
        }
        pixel++;
      }
    }

    m.convertTo(m, CV_8UC1, 255);
  }

  m.convertTo(m, CV_32F, 1 / 255.0);
  m.convertTo(m, -1, 1, -occlusion_thresh / 255.0);
  m.convertTo(m, -1, occlusion_contrast, occlusion_thresh / 255.0);
  m.convertTo(m, CV_8U, 255, occlusion_bright);
  GaussianBlur(m, m, Size(occlusion_blur * 2 + 1, occlusion_blur * 2 + 1), 0, 0);

  m.convertTo(m, CV_GRAY2RGB, 1);
  return m;
}

Mat ImageProcessor::modify_parallax() {
  Mat m;

  int threshType = !parallax_invert ? THRESH_BINARY_INV : THRESH_BINARY;
  Mat shape = getStructuringElement(MORPH_RECT,
                                    Size(abs(parallax_erode_dilate) * 2 + 1,
                                         abs(parallax_erode_dilate) * 2 + 1));

  switch (parallax_type) {
  case ParallaxType::Binary:
    m_parallax.copyTo(m);
    GaussianBlur(m, m, Size(parallax_focus * 2 + 1, parallax_focus * 2 + 1), 0,
                 0);
    threshold(m, m, parallax_max, 255, threshType);
    m -= parallax_min;
    if (parallax_erode_dilate > 0) {
      dilate(m, m, shape);
    } else {
      erode(m, m, shape);
    }
    GaussianBlur(m, m, Size(parallax_soft * 2 + 1, parallax_soft * 2 + 1), 0,
                 0);
    break;
  case ParallaxType::HeightMap:
    current_heightmap.copyTo(m);
    cvtColor(m, m, CV_RGBA2GRAY);
    m.convertTo(m, CV_32F, 1 / 255.0, -0.5);
    m.convertTo(m, -1, parallax_contrast, 0.5);
    m.convertTo(m, CV_8U, 255, parallax_brightness);
    GaussianBlur(m, m, Size(parallax_soft * 2 + 1, parallax_soft * 2 + 1), 0,
                 0);
    if (threshType == THRESH_BINARY_INV) {
      subtract(Scalar::all(255), m, m);
    }
    break;
  case ParallaxType::Intervals:
    break;
  case ParallaxType::Quantization:
    current_heightmap.copyTo(m);

    GaussianBlur(m, m, Size(parallax_focus * 2 + 1, parallax_focus * 2 + 1), 0,
                 0);
    m /= (parallax_quantization / 255.0);
    m *= (255.0 / parallax_quantization);
    GaussianBlur(m, m, Size(parallax_soft * 2 + 1, parallax_soft * 2 + 1), 0,
                 0);

    m = -255 / (parallax_max - parallax_min + 1) * parallax_min +
        255 / (parallax_max - parallax_min + 1) * m;

    if (threshType == THRESH_BINARY_INV) {
      subtract(Scalar::all(255), m, m);
    }
    break;
  }
  return m;
}

Mat ImageProcessor::modify_specular() {
  Mat m;

  m_specular.copyTo(m);

  m.convertTo(m, CV_32F, 1 / 255.0);
  cvtColor(m, m, CV_RGBA2GRAY);
  m.convertTo(m, -1, 1, -specular_thresh / 255.0);
  m.convertTo(m, -1, specular_contrast, specular_thresh / 255.0);
  m.convertTo(m, CV_8U, 255, specular_bright);
  GaussianBlur(m, m, Size(specular_blur * 2 + 1, specular_blur * 2 + 1), 0, 0);

  if (specular_invert) {
    subtract(Scalar::all(255), m, m);
  }

  return m;
}

void ImageProcessor::set_normal_bisel_blur_radius(int radius) {
  normal_bisel_blur_radius = radius;
  QtConcurrent::run(this,&ImageProcessor::generate_normal_map,false,true,false,QRect(0,0,0,0));
}

void ImageProcessor::generate_normal_map(bool updateEnhance, bool updateBump, bool updateDistance, QRect rect) {

  if (normal_counter > 2 || !active){
    return;
  }
  normal_counter++;

  QMutexLocker locker(&normal_mutex);

  /* Calculate rects to update */

  QList <QRect> rlist;

  bool diagonal = true;

  // Adjust for 1px blur
  rect.adjust(0,0,1,1);

  rect.moveTo((rect.left() % texture.width())-1, (rect.top() % texture.height())-1);

  rlist.append(rect.intersected(texture.rect()));
  if (rect.right() > texture.rect().right() && tileX){
    rlist.prepend(QRect(0, rect.top(), rect.right() % texture.width(), rect.height()).intersected(texture.rect()));
  } else {
    diagonal = false;
  }
  if (rect.bottom() > texture.rect().bottom() && tileY){
    rlist.append(QRect(rect.left(), 0, rect.width(),rect.bottom() % texture.height()).intersected(texture.rect()));
  } else {
    diagonal = false;
  }

  if (diagonal){
    rlist.append(QRect(0,0,rect.right() % texture.width(),rect.bottom() % texture.height()).intersected(texture.rect()));
  }

  rlist.removeAll(QRect(0,0,0,0));

  if (rlist.count() == 0){
    rlist.append(QRect(0,0,0,0));
  }
  QImage heightOverlay = get_heightmap_overlay();
  Mat heightmapOverlay = Mat(heightOverlay.height(), heightOverlay.width(), CV_8UC4, heightOverlay.scanLine(0));
  cvtColor(heightmapOverlay,heightmapOverlay,CV_RGBA2GRAY);
  heightmapOverlay.convertTo(heightmapOverlay,CV_32FC1,255);
  //add(heightmapOverlay,aux_height_ov,aux_height_ov);

  for (int i=0; i < rlist.count(); i++){
    calculate_normal(heightmapOverlay, m_height_ov,1,1,rlist.at(i));
  }

  if (updateEnhance) enhance_requested++;
  if (updateBump) bump_requested++;
  if (updateDistance) distance_requested++;

  if (update_tileable){
    update_tileable = false;
    distance_requested = true;
    set_current_heightmap();
    calculate_heightmap();
    calculate_distance();
  }
  if (enhance_requested){
    enhance_requested--;
    for (int i=0; i < rlist.count(); i++)
      calculate_normal(m_gray, m_emboss_normal, normal_depth, normal_blur_radius);
  }

  if (distance_requested){
    distance_requested--;
    new_distance = modify_distance();
  }

  if (bump_requested){
    bump_requested--;
    for (int i=0; i < rlist.count(); i++)
      calculate_normal(new_distance, m_distance_normal, normal_bisel_depth*normal_bisel_distance
                       , normal_bisel_blur_radius);
  }

  Mat normals;
  if (m_normal.rows == 0 || m_normal.cols == 0){
    m_emboss_normal.copyTo(m_normal);
    m_normal.convertTo(m_normal, CV_8UC3, 255);
  }


  normals = (m_emboss_normal*3/2.0 + m_distance_normal*3/2.0 + m_height_ov );

  foreach (QRect rect, rlist){

    int xmin = 0, xmax = normals.cols-1;
    int ymin = 0, ymax = normals.rows-1;

    if (rect != QRect(0,0,0,0)){
      rect.getCoords(&xmin, &ymin, &xmax, &ymax);
    }

    for (int x = xmin; x <= xmax; ++x) {
      int xaux = x;

      for (int y = ymin; y <= ymax; ++y) {
        int yaux = y;

        QColor oColor = normalOverlay.pixelColor(xaux,yaux);
        Vec3f overlay(oColor.redF()*2-1,oColor.greenF()*2-1,oColor.blueF()*2-1);
        Vec3f n = normalize((normals.at<Vec3f>(yaux, xaux))*(1-oColor.alphaF())+overlay*oColor.alphaF());

        n = n * 0.5 + Vec3f(0.5, 0.5, 0.5);
        m_normal.at<Vec3b>(yaux,xaux)[0] = n[0]*255;
        m_normal.at<Vec3b>(yaux,xaux)[1] = n[1]*255;
        m_normal.at<Vec3b>(yaux,xaux)[2] = n[2]*255;

        if (!active) {
          normal_counter--;
          return;
        }
      }
    }
  }

  normal_ready.lock();
  normal = QImage(static_cast<unsigned char *>(m_normal.data), m_normal.cols,
                  m_normal.rows, m_normal.step, QImage::Format_RGB888);
  normal_ready.unlock();
  processed();

  normal_counter--;
}

void ImageProcessor::calculate_normal(Mat mat, Mat src, int depth, int blur_radius, QRect r) {

  Mat aux;

  Rect rect(m_img.cols, m_img.rows, m_img.cols, m_img.rows);

  float dx, dy;
  int br = blur_radius * 2 + 1;

  if (mat.cols == m_img.cols *3){
    GaussianBlur(mat, aux, Size(br, br), 0);
  } else {
    copyMakeBorder(mat,aux,br,br,br,br,BORDER_WRAP);
    GaussianBlur(aux, aux, Size(br, br), 0);
    aux = aux(Rect(br,br,m_img.cols,m_img.rows));
  }
  int xs, xe, ys, ye;
  if (r == QRect(0,0,0,0)){
    xs = 0;
    xe = aux.rows-1;
    ys = 0;
    ye = aux.cols-1;
  } else {
    xs = r.top();
    xe = r.bottom();
    ys = r.left();
    ye = r.right();
  }

  Mat normals(aux.size(), CV_32FC3);
  if (tileable && normals.rows == m_img.rows * 3){
    src.copyTo(normals(rect));
  }else{
    src.copyTo(normals);
  }
  for (int x = xs; x <= xe; ++x) {
    for (int y = ys; y <= ye; ++y) {

      if (current_heightmap.at<Vec4b>(x, y)[3] == 0.0) {
        normals.at<Vec3f>(x,y) = Vec3f(0,0,1);

        continue;
      }
      if (x == 0) {
        dx = -3 * aux.at<float>(x, y) + 4 * aux.at<float>(x + 1, y) -
             aux.at<float>(x + 2, y);
      } else if (x == aux.rows - 1) {
        dx = 3 * aux.at<float>(x, y) - 4 * aux.at<float>(x - 1, y) +
             aux.at<float>(x - 2, y);
      } else {
        dx = -aux.at<float>(x - 1, y) + aux.at<float>(x + 1, y);
      }
      if (y == 0) {
        dy = -3 * aux.at<float>(x, y) + 4 * aux.at<float>(x, y+1) -
             aux.at<float>(x, y+2);
      } else if (y == aux.cols - 1) {
        dy = 3 * aux.at<float>(x, y) - 4 * aux.at<float>(x, y-1) +
             aux.at<float>(x, y-2);
      } else {
        dy = -aux.at<float>(x, y - 1) + aux.at<float>(x, y + 1);
      }
      Vec3f n = Vec3f(-dy * (depth / 1000.0) * normalInvertX,
                      dx * (depth / 1000.0) * normalInvertY, 1 * normalInvertZ);

      normals.at<Vec3f>(x, y) = n;

    }

  }
  if (tileable && normals.rows == m_img.rows * 3){
    normals(rect).copyTo(normals);

  }
  normals.copyTo(src);
}

void ImageProcessor::copy_settings(ProcessorSettings s) { settings = s; }

ProcessorSettings ImageProcessor::get_settings() { return settings; }

int ImageProcessor::get_normal_depth() { return normal_depth; }

int ImageProcessor::get_normal_blur_radius() { return normal_blur_radius; }

bool ImageProcessor::get_normal_bisel_soft() { return normal_bisel_soft; }

int ImageProcessor::get_normal_bisel_depth() { return normal_bisel_depth; }

int ImageProcessor::get_normal_bisel_distance() {
  return normal_bisel_distance;
}

int ImageProcessor::get_normal_bisel_blur_radius() {
  return normal_bisel_blur_radius;
}

int ImageProcessor::get_normal_invert_x() {
  return normalInvertX;
}

int ImageProcessor::get_normal_invert_y() {
  return normalInvertY;
}

QImage *ImageProcessor::get_texture() {
  return &texture;
}

QImage *ImageProcessor::get_normal() {
  if (normal_ready.tryLock(0)){
    last_normal = normal;
    normal_ready.unlock();
  }

  return &last_normal;
}

QImage *ImageProcessor::get_parallax() {
  if (parallax_ready.tryLock(0)){
    last_parallax = parallax;
    parallax_ready.unlock();
  }
  return &last_parallax;
}

QImage *ImageProcessor::get_specular() {
  if (specular_ready.tryLock(0)){
    last_specular = specular;
    specular_ready.unlock();
  }
  return &last_specular;
}

QImage *ImageProcessor::get_occlusion() {
  if (occlussion_ready.tryLock(0)){
    last_occlussion = occlussion;
    occlussion_ready.unlock();
  }
  return &last_occlussion;
}

QImage *ImageProcessor::get_texture_overlay() {
  QMutexLocker locker(&texture_overlay_mutex);
  return &textureOverlay;
}

void ImageProcessor::set_texture_overlay(QImage to){
  QMutexLocker locker(&texture_overlay_mutex);
  textureOverlay = to;
}

QImage *ImageProcessor::get_normal_overlay() {
  QMutexLocker locker(&normal_overlay_mutex);
  return &normalOverlay;
}

void ImageProcessor::set_normal_overlay(QImage no){
  QMutexLocker locker(&normal_overlay_mutex);
  normalOverlay = no;
}

QImage ImageProcessor::get_parallax_overlay() {
  QMutexLocker locker(&parallax_overlay_mutex);
  return parallaxOverlay;
}

void ImageProcessor::set_parallax_overlay(QImage po){
  QMutexLocker locker(&parallax_overlay_mutex);
  parallaxOverlay = po;
}

QImage ImageProcessor::get_specular_overlay() {
  QMutexLocker locker(&specular_overlay_mutex);
  return specularOverlay;
}

void ImageProcessor::set_specular_overlay(QImage so){
  QMutexLocker locker(&specular_overlay_mutex);
  specularOverlay = so;
}

QImage ImageProcessor::get_heightmap_overlay(){
  QMutexLocker locker(&heightmap_overlay_mutex);
  return heightOverlay;
}

void ImageProcessor::set_heightmap_overlay(QImage ho){
  QMutexLocker locker(&heightmap_overlay_mutex);
  heightOverlay = ho;
}

QImage ImageProcessor::get_occlusion_overlay() {
  QMutexLocker locker(&occlussion_overlay_mutex);
  return occlussionOverlay;
}

void ImageProcessor::set_occlussion_overlay(QImage oo){
  QMutexLocker locker(&occlussion_overlay_mutex);
  occlussionOverlay = oo;
}

bool ImageProcessor::get_parallax_invert() {
  return parallax_invert;
}

void ImageProcessor::set_parallax_invert(bool invert) {
  parallax_invert = invert;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

void ImageProcessor::set_parallax_focus(int focus) {
  parallax_focus = focus;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_focus() { return parallax_focus; }

void ImageProcessor::set_parallax_soft(int soft) {
  parallax_soft = soft;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_soft() { return parallax_soft; }

int ImageProcessor::get_parallax_thresh() { return parallax_max; }

void ImageProcessor::set_parallax_thresh(int thresh) {
  parallax_max = thresh;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_min() { return parallax_min; }

void ImageProcessor::set_parallax_min(int min) {
  parallax_min = min;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

ParallaxType ImageProcessor::get_parallax_type() { return parallax_type; }

void ImageProcessor::set_parallax_type(ParallaxType ptype) {
  parallax_type = ptype;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_quantization() {
  return parallax_quantization;
}

void ImageProcessor::set_parallax_quantization(int q) {
  parallax_quantization = q;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

void ImageProcessor::set_parallax_erode_dilate(int value) {
  parallax_erode_dilate = value;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_erode_dilate() {
  return parallax_erode_dilate;
}

void ImageProcessor::set_parallax_contrast(int contrast) {
  parallax_contrast = contrast / 1000.0;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

double ImageProcessor::get_parallax_contrast() { return parallax_contrast; }

void ImageProcessor::set_parallax_brightness(int brightness) {
  parallax_brightness = brightness;
  QtConcurrent::run(this,&ImageProcessor::calculate_parallax);
}

int ImageProcessor::get_parallax_brightness() { return parallax_brightness; }

void ImageProcessor::set_specular_blur(int blur) {
  specular_blur = blur;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}
int ImageProcessor::get_specular_blur() { return specular_blur; }

void ImageProcessor::set_specular_bright(int bright) {
  specular_bright = bright;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}
int ImageProcessor::get_specular_bright() { return specular_bright; }

void ImageProcessor::set_specular_invert(bool invert) {
  specular_invert = invert;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}
bool ImageProcessor::get_specular_invert() { return specular_invert; }

void ImageProcessor::set_specular_thresh(int thresh) {
  specular_thresh = thresh;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}
int ImageProcessor::get_specular_trhesh() { return specular_thresh; }

void ImageProcessor::set_specular_contrast(int contrast) {
  specular_contrast = contrast / 1000.0;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}
double ImageProcessor::get_specular_contrast() { return specular_contrast; }

void ImageProcessor::set_specular_base_color(Vec4b color) {
  specular_base_color = color;
  QtConcurrent::run(this,&ImageProcessor::calculate_specular);
}

Vec4b ImageProcessor::get_specular_base_color() { return specular_base_color; }

void ImageProcessor::set_occlusion_blur(int blur) {
  occlusion_blur = blur;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

int ImageProcessor::get_occlusion_blur() { return occlusion_blur; }

void ImageProcessor::set_occlusion_bright(int bright) {
  occlusion_bright = bright;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

int ImageProcessor::get_occlusion_bright() { return occlusion_bright; }

void ImageProcessor::set_occlusion_invert(bool invert) {
  occlusion_invert = invert;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

bool ImageProcessor::get_occlusion_invert() { return occlusion_invert; }

void ImageProcessor::set_occlusion_thresh(int thresh) {
  occlusion_thresh = thresh;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

int ImageProcessor::get_occlusion_trhesh() { return occlusion_thresh; }

void ImageProcessor::set_occlusion_contrast(int contrast) {
  occlusion_contrast = contrast / 1000.0;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

double ImageProcessor::get_occlusion_contrast() { return occlusion_contrast; }

void ImageProcessor::set_occlusion_distance_mode(bool distance_mode) {
  occlusion_distance_mode = distance_mode;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

bool ImageProcessor::get_occlusion_distance_mode() {
  return occlusion_distance_mode;
}

void ImageProcessor::set_occlusion_distance(int distance) {
  occlusion_distance = distance;
  QtConcurrent::run(this,&ImageProcessor::calculate_occlusion);
}

int ImageProcessor::get_occlusion_distance() { return occlusion_distance; }

ProcessorSettings &ProcessorSettings::operator=(ProcessorSettings other) {
  *tileable = *(other.tileable);
  *gradient_end = *(other.gradient_end);
  *parallax_max = *(other.parallax_max);
  *parallax_min = *(other.parallax_min);
  *parallax_soft = *(other.parallax_soft);
  *parallax_type = *(other.parallax_type);
  *parallax_focus = *(other.parallax_focus);
  *parallax_invert = *(other.parallax_invert);
  *parallax_contrast = *(other.parallax_contrast);
  *parallax_brightness = *(other.parallax_brightness);
  *parallax_erode_dilate = *(other.parallax_erode_dilate);
  *parallax_quantization = *(other.parallax_quantization);

  *normal_depth = *(other.normal_depth);
  *normalInvertX = *(other.normalInvertX);
  *normalInvertY = *(other.normalInvertY);
  *normalInvertZ = *(other.normalInvertZ);
  *normal_bisel_soft = *(other.normal_bisel_soft);
  *normal_bisel_depth = *(other.normal_bisel_depth);
  *normal_blur_radius = *(other.normal_blur_radius);
  *normal_bisel_distance = *(other.normal_bisel_distance);
  *normal_bisel_blur_radius = *(other.normal_bisel_blur_radius);

  *specular_blur = *(other.specular_blur);
  *specular_bright = *(other.specular_bright);
  *specular_invert = *(other.specular_invert);
  *specular_thresh = *(other.specular_thresh);
  *specular_contrast = *(other.specular_contrast);

  *occlusion_blur = *(other.occlusion_blur);
  *occlusion_bright = *(other.occlusion_bright);
  *occlusion_invert = *(other.occlusion_invert);
  *occlusion_thresh = *(other.occlusion_thresh);
  *occlusion_contrast = *(other.occlusion_contrast);
  *occlusion_distance = *(other.occlusion_distance);
  *occlusion_distance_mode = *(other.occlusion_distance_mode);

  lightList->clear();
  foreach (LightSource *light, *(other.lightList)) {
    LightSource *l = new LightSource();
    l->copy_settings(light);
    lightList->append(l);
  }
  return *this;
}

QImage ImageProcessor::get_heightmap() {
  Mat m;
  cvtColor(current_heightmap, m, CV_RGBA2GRAY);
  cvtColor(m, m, CV_GRAY2RGB);
  m.convertTo(m, CV_8UC3, 1);
  GaussianBlur(m, m,
               Size(normal_blur_radius * 2 + 1, normal_blur_radius * 2 + 1), 0);
  return QImage(static_cast<unsigned char *>(m.data), m.cols, m.rows, m.step,
                QImage::Format_RGB888);
}

QImage ImageProcessor::get_distance_map() {
  Mat m;
  cvtColor(new_distance, m, CV_GRAY2RGBA);
  m.convertTo(m, CV_8UC4, 255);
  return QImage(static_cast<unsigned char *>(m.data), m.cols, m.rows, m.step,
                QImage::Format_RGBA8888_Premultiplied);
}

void ImageProcessor::set_light_list(QList<LightSource *> &list) {
  lightList.clear();
  foreach (LightSource *light, list) {
    LightSource *l = new LightSource();
    l->copy_settings(light);
    lightList.append(l);
  }
}

QList<LightSource *> *ImageProcessor::get_light_list_ptr() {
  return &lightList;
}

void ImageProcessor::set_position(QVector3D new_pos) { position = new_pos; }

QVector3D *ImageProcessor::get_position() { return &position; }

void ImageProcessor::set_offset(QVector3D new_off) { offset = new_off; }

QVector3D *ImageProcessor::get_offset() { return &offset; }

void ImageProcessor::set_selected(bool s) { selected = s; }

bool ImageProcessor::get_selected() { return selected; }

void ImageProcessor::set_zoom(float new_zoom) { zoom = new_zoom; }

float ImageProcessor::get_zoom() { return zoom; }

void ImageProcessor::set_sx(float new_sx) { sx = new_sx; }

float ImageProcessor::get_sx() { return sx; }

void ImageProcessor::set_sy(float new_sy) { sy = new_sy; }

float ImageProcessor::get_sy() { return sy; }

void ImageProcessor::set_tile_x(bool tx) {
  tileX = tx;
  processed();
}

bool ImageProcessor::get_tile_x() { return tileX; }

void ImageProcessor::set_tile_y(bool ty) {
  tileY = ty;
  processed();
}

bool ImageProcessor::get_tile_y() { return tileY; }

void ImageProcessor::set_is_parallax(bool p) {
  is_parallax = p;
  processed();
}

bool ImageProcessor::get_is_parallax() { return is_parallax; }

void ImageProcessor::set_connected(bool c) { connected = c; }

bool ImageProcessor::get_connected() { return connected; }
