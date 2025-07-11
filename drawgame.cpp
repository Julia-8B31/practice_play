#include "drawgame.h"
#include "ui_drawgame.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QDebug>
#include <QNetworkInterface>
#include <QInputDialog>
#include <QBuffer>

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

void DrawingArea::handleMousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        lastPoint = event->pos();
        drawing = true;
    }
}

void DrawingArea::handleMouseMoveEvent(QMouseEvent *event) {
    if ((event->buttons() & Qt::LeftButton) && drawing) {
        publicDrawLineTo(event->pos());
    }
}

void DrawingArea::publicDrawLineTo(const QPoint &endPoint) {
    QPainter painter(&image);
    QColor currentColor = eraserMode ? Qt::white : penColor;
    painter.setPen(QPen(currentColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(lastPoint, endPoint);

    int adjust = penWidth * 2;
    QRect rect = QRect(lastPoint, endPoint).normalized().adjusted(-adjust, -adjust, adjust, adjust);
    update(rect);

    lastPoint = endPoint;
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
    handleMousePressEvent(event);
}

void DrawingArea::mouseMoveEvent(QMouseEvent *event)
{
    handleMouseMoveEvent(event);
}

void DrawingArea::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && drawing) {
        publicDrawLineTo(event->pos());
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

void DrawingArea::setImage(const QImage& newImage)
{
    image = newImage;
    update();
}

DrawGame::DrawGame(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DrawGame),
    gameTimer(new QTimer(this)),
    isDrawer(false),
    secondsLeft(180),
    server(nullptr),
    clientSocket(nullptr),
    isServer(false)
{
    ui->setupUi(this);

    wordList << "Кошка" << "Собака" << "Дом" << "Солнце" << "Дерево"
             << "Машина" << "Река" << "Гора" << "Книга" << "Цветок";

    QLayoutItem* item = ui->horizontalLayout->takeAt(0);
    if (item) {
        delete item->widget();
        delete item;
    }

    drawingArea = new DrawingArea(this);
    drawingArea->setMinimumSize(600, 400);
    ui->horizontalLayout->insertWidget(0, drawingArea);

    gameTimer->setInterval(1000);

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Выбор режима",
                                                              "Запустить сервер?", QMessageBox::Yes|QMessageBox::No);
    isServer = (reply == QMessageBox::Yes);

    if (isServer) {
        server = new QTcpServer(this);
        if (!server->listen(QHostAddress::Any, 12345)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось запустить сервер!");
            return;
        }

        QString ipAddress;
        QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
        for (const QHostAddress &address : ipAddressesList) {
            if (address != QHostAddress::LocalHost && address.toIPv4Address()) {
                ipAddress = address.toString();
                break;
            }
        }
        if (ipAddress.isEmpty())
            ipAddress = QHostAddress(QHostAddress::LocalHost).toString();

        ui->statusLabel->setText(tr("Сервер запущен на %1:%2. Ожидание подключения...")
                                     .arg(ipAddress).arg(server->serverPort()));

        connect(server, &QTcpServer::newConnection, this, &DrawGame::newConnection);
        isDrawer = true;
    } else {
        bool ok;
        QString host = QInputDialog::getText(this, "Подключение к серверу",
                                             "Введите IP сервера:", QLineEdit::Normal,
                                             "127.0.0.1", &ok);
        if (!ok || host.isEmpty()) return;

        clientSocket = new QTcpSocket(this);
        connect(clientSocket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            QMessageBox::critical(this, "Ошибка", "Ошибка подключения: " + clientSocket->errorString());
        });

        clientSocket->connectToHost(host, 12345);

        connect(clientSocket, &QTcpSocket::connected, this, [this]() {
            ui->statusLabel->setText("Подключено к серверу");
            isDrawer = false;
        });

        connect(clientSocket, &QTcpSocket::readyRead, this, &DrawGame::readData);
        connect(clientSocket, &QTcpSocket::disconnected, this, &DrawGame::disconnected);
    }

    setupConnections();
    onStartGameClicked();
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
    secondsLeft = 180;
    ui->statusLabel->setText("Статус: Игра началась! Время: 3:00");
    drawingArea->clear();

    if (isDrawer) {
        ui->wordLabel->setText("Слово: " + currentWord);
        if (clientSocket) {
            sendData("WORD:" + currentWord);
            sendData("ROLE:DRAWER");
        }
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

        if (clientSocket) {
            sendData("CHAT:" + message);
        }

        if (!isDrawer) {
            if (message.compare(currentWord, Qt::CaseInsensitive) == 0) {
                gameTimer->stop();
                ui->chatTextEdit->append("Система: Слово угадано! Это было \"" + currentWord + "\"");
                QMessageBox::information(this, "Поздравляем!", "Вы угадали слово: " + currentWord);

                if (clientSocket) {
                    sendData("WIN:" + message);
                    sendData("ROLE:DRAWER");
                }

                switchRoles();
                onStartGameClicked();
            } else {
                ui->chatTextEdit->append("Система: Неверно! Попробуйте ещё.");
            }
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
    if (clientSocket) {
        sendData("CLEAR:");
    }
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

    if (secondsLeft <= 0) {
        gameTimer->stop();
        QMessageBox::information(this, "Время вышло!", "Слово было: " + currentWord);
        switchRoles();
        onStartGameClicked();
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
    isDrawer = !isDrawer;
    ui->startButton->setText(isDrawer ? "Начать игру (Вы рисуете)" : "Начать игру (Вы отгадываете)");
}

void DrawGame::newConnection()
{
    if (clientSocket) {
        clientSocket->disconnectFromHost();
        delete clientSocket;
    }

    clientSocket = server->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, &DrawGame::readData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &DrawGame::disconnected);

    ui->statusLabel->setText("Клиент подключен");
    isDrawer = true;
    onStartGameClicked();
}

void DrawGame::readData()
{
    while (clientSocket->bytesAvailable() > 0) {
        QByteArray data = clientSocket->readAll();
        QString strData = QString::fromUtf8(data);

        QStringList messages = strData.split("\n", Qt::SkipEmptyParts);
        for (const QString &message : messages) {
            int separatorIndex = message.indexOf(':');
            if (separatorIndex == -1) continue;

            QString command = message.left(separatorIndex);
            QString dataPart = message.mid(separatorIndex + 1);

            if (command == "DRAW") {
                QStringList parts = dataPart.split(';');
                if (parts.size() >= 3) {
                    QStringList start = parts[0].split(',');
                    QStringList end = parts[1].split(',');
                    QStringList params = parts[2].split(',');

                    if (start.size() == 2 && end.size() == 2 && params.size() >= 4) {
                        bool ok1, ok2, ok3, ok4;
                        QPoint startPoint(start[0].toInt(&ok1), start[1].toInt(&ok2));
                        QPoint endPoint(end[0].toInt(&ok3), end[1].toInt(&ok4));

                        if (!ok1 || !ok2 || !ok3 || !ok4) continue;

                        QColor color(params[0].toInt(), params[1].toInt(), params[2].toInt());
                        bool eraser = params[3].toInt();
                        int width = params.size() > 4 ? params[4].toInt() : 3;

                        QColor oldColor = drawingArea->getPenColor();
                        bool oldEraser = drawingArea->isEraserMode();
                        int oldWidth = drawingArea->getPenWidth();

                        drawingArea->setPenColor(color);
                        drawingArea->setEraserMode(eraser);
                        drawingArea->setPenWidth(width);
                        drawingArea->setLastPoint(startPoint);
                        drawingArea->publicDrawLineTo(endPoint);

                        drawingArea->setPenColor(oldColor);
                        drawingArea->setEraserMode(oldEraser);
                        drawingArea->setPenWidth(oldWidth);
                    }
                }
            }
            else if (command == "CLEAR") {
                drawingArea->clear();
            }
            else if (command == "WORD") {
                currentWord = dataPart;
                if (!isDrawer) {
                    ui->wordLabel->setText("Слово: *****");
                } else {
                    ui->wordLabel->setText("Слово: " + currentWord);
                }
            }
            else if (command == "ROLE") {
                isDrawer = (dataPart == "DRAWER");
                ui->startButton->setText(isDrawer ? "Начать игру (Вы рисуете)" : "Начать игру (Вы отгадываете)");
            }
            else if (command == "CHAT") {
                ui->chatTextEdit->append("Соперник: " + dataPart);
            }
            else if (command == "WIN") {
                gameTimer->stop();
                currentWord = dataPart;
                ui->chatTextEdit->append("Система: Соперник угадал слово \"" + currentWord + "\"");
                QMessageBox::information(this, "Игра окончена", "Соперник угадал слово: " + currentWord);
                switchRoles();
                onStartGameClicked();
            }
        }
    }
}

void DrawGame::disconnected()
{
    ui->statusLabel->setText("Соединение разорвано");
    if (clientSocket) {
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
}

void DrawGame::sendData(const QString &data)
{
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        clientSocket->write((data + "\n").toUtf8());
    }
}

void DrawGame::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && isDrawer) {
        drawingArea->handleMousePressEvent(event);
        if (clientSocket) {
            QString data = QString("DRAW:%1,%2;%1,%2;%3,%4,%5,%6,%7")
            .arg(event->pos().x()).arg(event->pos().y())
                .arg(drawingArea->getPenColor().red())
                .arg(drawingArea->getPenColor().green())
                .arg(drawingArea->getPenColor().blue())
                .arg(drawingArea->isEraserMode() ? 1 : 0)
                .arg(drawingArea->getPenWidth());
            sendData(data);
        }
    }
}

void DrawGame::mouseMoveEvent(QMouseEvent *event) {
    if ((event->buttons() & Qt::LeftButton) && isDrawer) {
        drawingArea->handleMouseMoveEvent(event);
        if (clientSocket) {
            QString data = QString("DRAW:%1,%2;%3,%4;%5,%6,%7,%8,%9")
            .arg(drawingArea->getLastPoint().x()).arg(drawingArea->getLastPoint().y())
                .arg(event->pos().x()).arg(event->pos().y())
                .arg(drawingArea->getPenColor().red())
                .arg(drawingArea->getPenColor().green())
                .arg(drawingArea->getPenColor().blue())
                .arg(drawingArea->isEraserMode() ? 1 : 0)
                .arg(drawingArea->getPenWidth());
            sendData(data);
        }
    }
}
