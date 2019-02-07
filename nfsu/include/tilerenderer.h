#pragma once
#include <QtWidgets/qopenglwidget.h>
#include <QtOpenGL/qglshaderprogram.h>
#include <QtOpenGL/qglbuffer.h>
#include <QtGui/qopengltexture.h>
#include "texture.h"

namespace nfsu {

	enum class TilePaintTool {
		BRUSH, LINE, SQUARE, FILL, EYEDROPPER
	};

	class PaletteRenderer;

	class TileRenderer : public QOpenGLWidget {

	public:

		TileRenderer(PaletteRenderer *palette);
		~TileRenderer();

		void setTexture(nfs::Texture2D texture);
		nfs::Texture2D getTexture();

		void usePalette(bool b);
		void setEditable(bool b);
		
		void setCursorSize(u32 size);

		//If the image is 4-bit, use as y offset into palette
		void setPaletteOffset(u8 y);

		void setPaintTool(TilePaintTool tool);

		void initializeGL() override;
		void paintGL() override;
		void mouseMoveEvent(QMouseEvent *e) override;
		void mousePressEvent(QMouseEvent *e) override;
		void wheelEvent(QWheelEvent *e) override;
		void mouseReleaseEvent(QMouseEvent *e) override;
		void keyPressEvent(QKeyEvent *e) override;
		void keyReleaseEvent(QKeyEvent *e) override;

		void drawPoint(QPoint point, u32 size = 0 /* uses cursorSize by default */);
		void drawLine(QPoint p0, QPoint p1, u32 size = 0 /* uses cursorSize by default */);
		void drawSquare(QPoint p0, QPoint p2);
		void fill(QPoint p0);

		void fill(i32 x, i32 y, u32 mask);
		u32 get(QPoint p0);

		QPoint globalToTexture(QPoint pos);

		u32 getSelectedPalette();

		//Re-initialize texture & repaint
		void updateTexture();

		void reset();
		void updateScale();

		void resizeEvent(QResizeEvent *e) override;

	protected:

		void setupGTexture();
		void destroyGTexture();

	private:


		u32 cursorSize = 1, specialKey = 0;
		bool palette = true, editable = true, isMouseDown = false, isLeft = false;
		TilePaintTool tool = TilePaintTool::BRUSH;

		u8 yOffset = 0;

		PaletteRenderer *paletteRenderer;
		nfs::Texture2D texture;

		QPoint prev;
		QVector2D offset = { 0, 0 }, scale = { 1, 1 };

		QGLShaderProgram shader;
		QGLBuffer quadVBO;

		QOpenGLTexture *tiledTexture = nullptr, *magicTexture = nullptr;

	};

}