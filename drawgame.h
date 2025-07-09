#ifndef DRAWGAME_H
#define DRAWGAME_H

#include <QMainWindow>
#include <QList>
#include <QString>
#include <QColor>
#include <QPoint>
#include <QImage>
#include <QTimer>

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


protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawLineTo(const QPoint &endPoint);
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

private slots:
    void onStartGameClicked();
    void onSendMessageClicked();
    void onColorChanged(int index);
    void onClearClicked();
    void onEraserClicked(bool checked);
    void updateGame();

private:
    void setupConnections();
    void generateRandomWord();
    void switchRoles(); // Переключение ролей между игроками

    Ui::DrawGame *ui;
    DrawingArea *drawingArea;
    QTimer *gameTimer;
    QString currentWord;
    QStringList wordList;
    bool isDrawer; // true - игрок рисует, false - отгадывает
    int secondsLeft; // Оставшееся время
};

#endif // DRAWGAME_H
