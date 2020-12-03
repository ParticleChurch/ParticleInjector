#include "mainwindow.hpp"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    this->setWindowFlags(MainWindowFlags);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setWindowOpacity(1);
    this->setStyleSheet(("\
        background-color: rgb" + background.toString() + ";\
        color: white;\
    ").c_str());
    this->resize(400, 224);

    // center on primary monitor
    {
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);

        this->move((width - this->width()) / 2, (height - this->height()) / 2);
    }

    // minimize, drag to move, exit, etc.
    this->addTopbar();
    this->addContent();

    // animations
    this->animationTimer = new QTimer();
    this->animationTimer->setInterval(5);
    connect(this->animationTimer, &QTimer::timeout, this, /* TODO: use SLOT instead of this garbage: */ [=]() {this->animationUpdate(); });
    this->animationTimer->start();

    // init work
    this->worker = new Worker();
    connect(this->worker, &Worker::taskComplete, this, &MainWindow::workerTaskComplete);
    connect(this->worker, &Worker::taskDescription, this, &MainWindow::workerUpdateTaskDescription);
    connect(this->worker, &Worker::finished, this, &MainWindow::close);
    this->worker->start();

    Animation::changed(this->Tasks.at(0)->Spinner->AnimationId.c_str(), 1);
};

MainWindow::~MainWindow()
{
    delete this->closeButtonOverlay;
    delete this->closeButton;
    delete this->minimizeButtonOverlay;
    delete this->minimizeButton;
    delete this->topbar;
}

void MainWindow::addTopbar()
{
    // topbar
    this->topbar = new QWidget(this);
    this->topbar->setStyleSheet(("background-color: rgb" + this->topbarBackground.toString() + "; border: none;").c_str());
    this->topbar->resize(this->width(), 24);
    this->topbar->installEventFilter(this);

    // close button
    this->closeButton = new QPushButton(this->topbar);
    this->closeButtonOverlay = new Overlay("X", this->closeButton);
    this->closeButton->resize(30, 24);
    this->closeButton->setStyleSheet("background-color: rgba(0,0,0,1); border: none;");
    this->closeButton->show();
    this->closeButton->move(this->width() - this->closeButton->width(), 0);
    this->closeButton->installEventFilter(this);
    Animation::newAnimation("close-hovered", 0);

    // minimize button
    this->minimizeButton = new QPushButton(this->topbar);
    this->minimizeButtonOverlay = new Overlay("-", this->minimizeButton);
    this->minimizeButton->resize(30, 24);
    this->minimizeButton->setStyleSheet("background-color: rgba(0,0,0,1); border: none;");
    this->minimizeButton->show();
    this->minimizeButton->move(this->width() - this->closeButton->width() - this->minimizeButton->width(), 0);
    this->minimizeButton->installEventFilter(this);
    Animation::newAnimation("minimize-hovered", 0);

    // Label
    this->windowLabel = new QLabel("PARTICLE.CHURCH INJECTOR", this->topbar);
    this->windowLabel->move(7, 0);
    this->windowLabel->resize(this->width() - 67, this->topbar->height());
    this->windowLabel->setStyleSheet("color: rgb(120, 120, 120); font-size: 10pt; font-family: 'Arial'; font-weight: 700; font-style: italic");
};


void MainWindow::addContent()
{
    std::string TaskNames[]{
        "Ensuring Latest Injector Version",
        "Finding CS:GO Process",
        "Downloading Latest Cheat Version",
        "Injecting Cheat Into CS:GO",
    };

    for (int i = 0; i < 4; i++)
    {
        int y = this->topbar->height() + i * 50;
        std::string name = TaskNames[i];

        Task* t = new Task(name, this);
        t->move(0, y);
        t->resize(this->width(), 50);
        t->setup();

        this->Tasks.push_back(t);
    }
}

bool MainWindow::eventFilter(QObject* object, QEvent* e)
{
    if ((QPushButton*)object == this->closeButton)
    {
        switch (e->type())
        {
        case QEvent::Enter:
            Animation::changed("close-hovered", 1);
            return true;
        case QEvent::Leave:
            Animation::changed("close-hovered", 0);
            return true;
        case QEvent::MouseButtonPress:
            this->close();
            return true;
        default: // shut up warnings in qt creator
            break;
        }
    }
    else if ((QPushButton*)object == this->minimizeButton)
    {
        switch (e->type())
        {
        case QEvent::Enter:
            Animation::changed("minimize-hovered", 1);
            return true;
        case QEvent::Leave:
            Animation::changed("minimize-hovered", 0);
            return true;
        case QEvent::MouseButtonPress:
            this->setWindowState(Qt::WindowMinimized);
            return true;
        default: // shut up warnings in qt creator
            break;
        }
    }
    else if ((QWidget*)object == this->topbar)
    {
        QPoint mpos = QCursor::pos();
        switch (e->type())
        {
        case QEvent::MouseButtonPress:
        {
            this->isDraggingWindow = true;
            this->draggingOffet = (mpos - this->pos());
            return true;
        }
        case QEvent::MouseButtonRelease:
        {
            this->isDraggingWindow = false;
            return true;
        }
        case QEvent::MouseMove:
        {
            if (!this->isDraggingWindow)
                break;

            this->move(mpos - this->draggingOffet);
            return true;
        }
        default: // shut up warnings in qt creator
            break;
        }
    }

    // if we don't have a special case, do normal thing
    return QWidget::eventFilter(object, e);
}

void MainWindow::animationUpdate()
{
    Animation::Anim* a;
    if ((a = Animation::get("close-hovered")))
    {
        double timeSinceChange = Animation::age(a);
        float factor = Animation::animate(timeSinceChange, 0.1, Animation::Interpolation::linear);
        if (a->state == 0) factor = 1.f - factor;

        std::string css = "\
            background-color: rgb" + this->topbarBackground.lerp(this->closeButtonHovered, factor).toString() + ";\
            border: none;\
        ";

        this->closeButton->setStyleSheet(css.c_str());
    }
    if ((a = Animation::get("minimize-hovered")))
    {
        double timeSinceChange = Animation::age(a);
        float factor = Animation::animate(timeSinceChange, 0.1, Animation::Interpolation::linear);
        if (a->state == 0) factor = 1.f - factor;

        std::string css = "\
            background-color: rgb" + this->topbarBackground.lerp(this->minimizeButtonHovered, factor).toString() + ";\
            border: none;\
        ";

        this->minimizeButton->setStyleSheet(css.c_str());
    }

    for (int i = 0; i < 4; i++)
    {
        this->Tasks.at(i)->Spinner->repaint();
    }
}

void MainWindow::paintEvent(QPaintEvent* e)
{
    // todo: maybe draw an outline or something, to help with visibility
}

Overlay::Overlay(const char* type, QWidget* parent) : QWidget(parent) {
    this->move(0, 0);
    this->_overlay_type = type;
};
void Overlay::paintEvent(QPaintEvent* e)
{
    QWidget* parent = (QWidget*)this->parent();
    this->resize(parent->width(), parent->height());

    int size = 8; // pixels
    int x = (parent->width() - size) / 2;
    int y = (parent->height() - size) / 2;


    QPainter painter(this);


    switch (this->_overlay_type[0])
    {
    case 'x':
    case 'X':
    {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen line(Qt::white, 1.25f, Qt::SolidLine);
        painter.setPen(line);

        painter.drawLine(x, y, x + size, y + size);
        painter.drawLine(x + size, y, x, y + size);
        break;
    }
    case '-':
    case '_':
    {
        painter.setRenderHint(QPainter::Antialiasing, false);
        QPen line(Qt::white, 1, Qt::SolidLine);
        painter.setPen(line);

        painter.drawLine(x, y + size / 2, x + size, y + size / 2);
        break;
    }
    }

}

Task::Task(std::string Title, QWidget* parent) : QWidget(parent)
{
    this->Title = Title;
    this->Subtitle = "Waiting...";
}

void Task::setup()
{
    this->Spinner = new LoadingSpinner(this);
    this->Spinner->resize(this->height(), this->height());

    this->TitleLabel = new QLabel(this->Title.c_str(), this);
    this->SubtitleLabel = new QLabel(this->Subtitle.c_str(), this);

    this->TitleLabel->setStyleSheet("font-size: 12pt; font-family: 'Arial'; font-weight: 600; color: white;");
    this->SubtitleLabel->setStyleSheet("font-size: 10pt; font-family: 'Arial'; font-style: italic; color: rgb(150, 150, 150);");

    this->TitleLabel->move(this->Spinner->width(), 3);
    this->TitleLabel->resize(this->width() - this->Spinner->width(), this->height() / 2);
    this->TitleLabel->setAlignment(Qt::AlignBottom | Qt::AlignLeft);

    this->SubtitleLabel->move(this->Spinner->width(), 3 + this->height() / 2);
    this->SubtitleLabel->resize(this->width() - this->Spinner->width(), this->height() / 2);
    this->SubtitleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

LoadingSpinner::LoadingSpinner(QWidget* parent) : QWidget(parent)
{
    this->AnimationId = ((Task*)parent)->Title + "-spinner";
    Animation::newAnimation(this->AnimationId.c_str(), 1);
    Animation::changed(this->AnimationId.c_str(), 0);
}

void LoadingSpinner::paintEvent(QPaintEvent* e)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    Animation::Anim* a = Animation::get(this->AnimationId.c_str());
    constexpr int diameter = 26;
    constexpr int penWidth = 2;
    constexpr int pos = (50 - diameter + penWidth) / 2;

    if (a->state == 0) // cirle (not begun yet)
    {
        QPen line(Qt::white, penWidth, Qt::SolidLine);
        painter.setPen(line);
        painter.setBrush(QColor(0, 0, 0, 0));
        painter.drawEllipse(pos, pos, diameter - penWidth, diameter - penWidth);
    }
    else if (a->state == 1) // spinner (current task)
    {
        QPen line(Qt::white, penWidth, Qt::SolidLine);
        painter.setPen(line);
        painter.setBrush(QColor(0, 0, 0, 0));

        // 5750 = 1 full revolution, wtf are those units??
        double age = Animation::age(this->AnimationId.c_str());
        double animtime = age - floor(age);

        painter.drawArc(pos, pos, diameter - penWidth, diameter - penWidth, -animtime * 5750.0, 5750 / 4);
    }
    else if (a->state == 2) // checkmark (completed)
    {
        // background
        painter.setPen(QColor(0, 0, 0, 0));
        painter.setBrush(QColor(75, 160, 220, 255));
        painter.drawEllipse((50 - diameter) / 2, (50 - diameter) / 2, diameter, diameter);

        // checkmark
        QPen line(Qt::white, penWidth, Qt::SolidLine);
        painter.setPen(line);
        painter.setBrush(QColor(0, 0, 0, 0));

        QPoint points[]{
            QPoint(20, 25),
            QPoint(24, 30),
            QPoint(31, 21),
        };
        painter.drawPolyline(points, 3);
    }
    else if (a->state == 3) // checkmark (failed)
    {
        // background
        painter.setPen(QColor(0, 0, 0, 0));
        painter.setBrush(QColor(235, 100, 100, 255));
        painter.drawEllipse((50 - diameter) / 2, (50 - diameter) / 2, diameter, diameter);

        // X
        QPen line(Qt::white, 2, Qt::SolidLine);
        painter.setPen(line);
        painter.setBrush(QColor(0, 0, 0, 0));

        int cx = this->width() / 2;
        int cy = this->height() / 2;
        int off = 5;
        painter.drawLine(cx - off, cy - off, cx + off, cy + off);
        painter.drawLine(cx - off, cy + off, cx + off, cy - off);
    }
}

void MainWindow::workerUpdateTaskDescription(int taskNumber, QString newLabel)
{
    this->Tasks.at(taskNumber)->SubtitleLabel->setText(newLabel);
}

void MainWindow::workerTaskComplete(int taskNumber, bool success)
{
    Animation::changed(this->Tasks.at(taskNumber)->Spinner->AnimationId.c_str(), success ? 2 : 3);
    if (success && (taskNumber < this->Tasks.size()-1))
        Animation::changed(this->Tasks.at((size_t)taskNumber + (size_t)1)->Spinner->AnimationId.c_str(), 1);
}