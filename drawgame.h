#ifndef DRAWGAME_H
#define DRAWGAME_H

#include <QMainWindow>
#include <QList>
#include <QString>
#include <QColor>
#include <QPoint>
#include <QImage>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>

namespace Ui {
class DrawGame;
}

class DrawingArea : public QWidget
{
    Q_OBJECT
public:
    explicit DrawingArea(QWidget *parent = nullptr);
    void setPenColor(const QColor &newColor);
    void setPenWidth(int newWidth);
    void clear();
    bool isEraserMode() const { return eraserMode; }
    void setEraserMode(bool mode) { eraserMode = mode; }
    const QImage& getImage() const { return image; }
    void setImage(const QImage& newImage);
    QColor getPenColor() const { return penColor; }
    int getPenWidth() const { return penWidth; }
    QPoint getLastPoint() const { return lastPoint; }
    void setLastPoint(const QPoint &point) { lastPoint = point; }
    void handleMousePressEvent(QMouseEvent *event);
    void handleMouseMoveEvent(QMouseEvent *event);
    void publicDrawLineTo(const QPoint &endPoint);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void resizeImage(QImage *image, const QSize &newSize);

    bool drawing;
    bool eraserMode;
    QColor penColor;
    int penWidth;
    QImage image;
    QPoint lastPoint;
};

class DrawGame : public QMainWindow
{
    Q_OBJECT

public:
    explicit DrawGame(QWidget *parent = nullptr);
    ~DrawGame();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onStartGameClicked();
    void onSendMessageClicked();
    void onColorChanged(int index);
    void onClearClicked();
    void onEraserClicked(bool checked);
    void updateGame();
    void newConnection();
    void readData();
    void disconnected();

private:
    void setupConnections();
    void generateRandomWord();
    void switchRoles();
    void sendData(const QString &data);
    void processDrawingCommand(const QString &data);
    void sendImageData();
    void assignRandomRole();

    Ui::DrawGame *ui;
    DrawingArea *drawingArea;
    QTimer *gameTimer;
    QString currentWord;
    QStringList wordList;
    bool isDrawer;
    int secondsLeft;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    bool isServer;
};

#endif // DRAWGAME_H
