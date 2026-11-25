#ifndef LOM_LAUNCHER_BACKDROP_H
#define LOM_LAUNCHER_BACKDROP_H

#include <QPixmap>
#include <QWidget>

namespace lom::launcher {
class Backdrop : public QWidget {
    Q_OBJECT

  public:
    /**
     * @brief Build the backdrop.
     * @param parent Parent widget.
     */
    explicit Backdrop(QWidget* parent);

  protected:
    /**
     * @brief Paint the background picture, filling the window and centered,
     *        with its shading. A gradient stands in when there's no picture.
     * @param event The event.
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Forget the scaled picture, so the next paint scales it again.
     * @param event The event.
     */
    void resizeEvent(QResizeEvent* event) override;

  private:
    QPixmap image_;
    QPixmap scaled_;
};

}

#endif
