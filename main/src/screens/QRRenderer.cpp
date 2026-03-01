#include "screens/QRRenderer.hpp"

lv_obj_t* QRRenderer::render(lv_obj_t* parent,
							 const uint8_t* data,
							 size_t         dataLen,
							 lv_coord_t     x,
							 lv_coord_t     y,
							 uint8_t        scale)
{
	return nullptr;
}

void QRRenderer::drawOnCanvas(lv_obj_t*     canvas,
							  const uint8_t* qrMatrix,
							  int            qrSize,
							  uint8_t        scale)
{}
