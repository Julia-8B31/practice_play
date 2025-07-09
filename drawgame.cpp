#include "drawgame.h"
#include "ui_drawgame.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QDebug>

DrawingArea::DrawingArea(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents);
    drawing = false;
    eraserMode = false;
    penColor = Qt::black;
    penWidth = 3;
    image = QImage(800, 600, QImage::Format_RGB32);
    image.fill(Qt::white);
}

void DrawingArea::setPenColor(const QColor &newColor)
{
    penColor = newColor;
}

void DrawingArea::setPenWidth(int newWidth)
{
    penWidth = newWidth;
}



void DrawingArea::clear()
{
    image.fill(Qt::white);
    update();
}

void DrawingArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        lastPoint = event->pos();
        drawing = true;
    }
}

void DrawingArea::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && drawing) {
        drawLineTo(event->pos());
    }
}

void DrawingArea::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && drawing) {
        drawLineTo(event->pos());
        drawing = false;
    }
}

void DrawingArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QRect dirtyRect = event->rect();
    painter.drawImage(dirtyRect, image, dirtyRect);
}

void DrawingArea::resizeEvent(QResizeEvent *event)
{
    if (width() > image.width() || height() > image.height()) {
        int newWidth = qMax(width() + 128, image.width());
        int newHeight = qMax(height() + 128, image.height());
        resizeImage(&image, QSize(newWidth, newHeight));
        update();
    }
    QWidget::resizeEvent(event);
}

void DrawingArea::drawLineTo(const QPoint &endPoint)
{
    QPainter painter(&image);
    QColor currentColor = eraserMode ? Qt::white : penColor;
    painter.setPen(QPen(currentColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(lastPoint, endPoint);

    int adjust = penWidth * 2;
    QRect rect = QRect(lastPoint, endPoint).normalized().adjusted(-adjust, -adjust, adjust, adjust);
    update(rect);

    lastPoint = endPoint;
}

void DrawingArea::resizeImage(QImage *image, const QSize &newSize)
{
    if (image->size() == newSize)
        return;

    QImage newImage(newSize, QImage::Format_RGB32);
    newImage.fill(Qt::white);
    QPainter painter(&newImage);
    painter.drawImage(QPoint(0, 0), *image);
    *image = newImage;
}

DrawGame::DrawGame(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DrawGame),
    gameTimer(new QTimer(this)),
    isDrawer(true), // Первый игрок - рисующий
    secondsLeft(180) // 3 минуты
{
    ui->setupUi(this);

    // Инициализация списка слов
    wordList << "Кошка" << "Собака" << "Дом" << "Солнце" << "Дерево"
             << "Машина" << "Река" << "Гора" << "Книга" << "Цветок";

    // Удаляем placeholder и добавляем DrawingArea
    QLayoutItem* item = ui->horizontalLayout->takeAt(0);
    if (item) {
        delete item->widget();
        delete item;
    }

    drawingArea = new DrawingArea(this);
    drawingArea->setMinimumSize(600, 400);
    ui->horizontalLayout->insertWidget(0, drawingArea);

    // Настройка таймера
    gameTimer->setInterval(1000); // 1 секунда

    setupConnections();
    onStartGameClicked(); // Начинаем игру сразу
    connect(ui->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            drawingArea, &DrawingArea::setPenWidth);
}

DrawGame::~DrawGame()
{
    delete ui;
}

void DrawGame::setupConnections()
{
    connect(ui->startButton, &QPushButton::clicked, this, &DrawGame::onStartGameClicked);
    connect(ui->sendButton, &QPushButton::clicked, this, &DrawGame::onSendMessageClicked);
    connect(ui->colorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DrawGame::onColorChanged);
    connect(ui->clearButton, &QPushButton::clicked, this, &DrawGame::onClearClicked);
    connect(ui->eraserButton, &QPushButton::toggled, this, &DrawGame::onEraserClicked);
    connect(gameTimer, &QTimer::timeout, this, &DrawGame::updateGame);
}

void DrawGame::onStartGameClicked()
{
    generateRandomWord();
    gameTimer->start();
    secondsLeft = 180; // Сброс таймера на 3 минуты
    ui->statusLabel->setText("Статус: Игра началась! Время: 3:00");
    drawingArea->clear();

    // Рисующий видит слово, отгадывающий - нет
    if (isDrawer) {
        ui->wordLabel->setText("Слово: " + currentWord);
    } else {
        ui->wordLabel->setText("Слово: *****");
    }
}

void DrawGame::onSendMessageClicked()
{
    QString message = ui->messageLineEdit->text().trimmed();
    if (!message.isEmpty()) {
        ui->chatTextEdit->append("Вы: " + message);
        ui->messageLineEdit->clear();

        // Проверка угадывания слова (для всех, кроме рисующего)
        if (!isDrawer) {
            if (message.compare(currentWord, Qt::CaseInsensitive) == 0) {
                gameTimer->stop();
                ui->chatTextEdit->append("Система: Слово угадано! Это было \"" + currentWord + "\"");
                QMessageBox::information(this, "Поздравляем!", "Вы угадали слово: " + currentWord);
                switchRoles();
                onStartGameClicked(); // Новая игра
                return; // Прерываем дальнейшую обработку
            } else {
                ui->chatTextEdit->append("Система: Неверно! Попробуйте ещё.");
            }
        } else {
            ui->chatTextEdit->append("Система: Вы сейчас рисуете, а не отгадываете.");
        }
    }
}

void DrawGame::onColorChanged(int index)
{
    QColor color;
    switch(index) {
    case 0: color = Qt::black; break;
    case 1: color = Qt::red; break;
    case 2: color = Qt::green; break;
    case 3: color = Qt::blue; break;
    case 4: color = Qt::yellow; break;
    default: color = Qt::black;
    }
    drawingArea->setPenColor(color);
    drawingArea->setEraserMode(false);
    ui->eraserButton->setChecked(false);
}

void DrawGame::onClearClicked()
{
    drawingArea->clear();
}

void DrawGame::onEraserClicked(bool checked)
{
    drawingArea->setEraserMode(checked);
    if (checked) {
        drawingArea->setPenColor(Qt::white);
    } else {
        onColorChanged(ui->colorComboBox->currentIndex());
    }
}

void DrawGame::updateGame()
{
    secondsLeft--;
    int minutes = secondsLeft / 60;
    int seconds = secondsLeft % 60;
    ui->statusLabel->setText(QString("Статус: Игра идет... Время: %1:%2")
                                 .arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')));

    // Время вышло
    if (secondsLeft <= 0) {
        gameTimer->stop();
        QMessageBox::information(this, "Время вышло!", "Слово было: " + currentWord);
        switchRoles(); // Меняем роли
        onStartGameClicked(); // Новая игра
    }
}

void DrawGame::generateRandomWord()
{
    if (!wordList.isEmpty()) {
        int randomIndex = QRandomGenerator::global()->bounded(wordList.size());
        currentWord = wordList.at(randomIndex);
    }
}

void DrawGame::switchRoles()
{
    isDrawer = !isDrawer; // Меняем роль
    ui->startButton->setText(isDrawer ? "Начать игру (Вы рисуете)" : "Начать игру (Вы отгадываете)");
}

