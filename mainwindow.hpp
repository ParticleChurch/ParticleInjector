#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QLabel>
#include <QDebug>

#include <string>
#include <algorithm>

#include "worker.hpp"
#include "animation.hpp"
#include "color.hpp"

constexpr Qt::WindowFlags MainWindowFlags = Qt::WindowType::FramelessWindowHint;

class LoadingSpinner : public QWidget
{
    Q_OBJECT
public:
    std::string AnimationId;

    LoadingSpinner(QWidget* parent);
    void paintEvent(QPaintEvent* e);
};

class Task : public QWidget
{
    Q_OBJECT
public:
    std::string Title;
    std::string Subtitle;
    QLabel* TitleLabel;
    QLabel* SubtitleLabel;
    LoadingSpinner* Spinner;

public:
    Task(std::string Title, QWidget* parent);
    void setup();
};

class Overlay : public QWidget
{
    Q_OBJECT
private:
    const char* _overlay_type = "x";

public:
    Overlay(const char* type, QWidget* parent);

    void paintEvent(QPaintEvent* e);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    QTimer* animationTimer;

    QWidget* topbar;
    QPushButton* closeButton;
    Overlay* closeButtonOverlay;
    QPushButton* minimizeButton;
    Overlay* minimizeButtonOverlay;
    QLabel* windowLabel;

    Worker* worker;

    bool isDraggingWindow = false;
    QPoint draggingOffet;

    Color background = Color(60, 60, 60);
    Color topbarBackground = Color(20, 20, 20);
    Color closeButtonHovered = Color(200, 50, 50);
    Color minimizeButtonHovered = Color(100, 100, 100);

    std::vector<Task*> Tasks;

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    bool eventFilter(QObject* object, QEvent* e);

    void addTopbar();
    void addContent();

    void paintEvent(QPaintEvent* e);

public slots:
    void animationUpdate();
    void workerTaskComplete(int taskNumber, bool success);
    void workerUpdateTaskDescription(int taskNumber, QString newLabel);
};
