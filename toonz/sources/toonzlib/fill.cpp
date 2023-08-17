

#include "trastercm.h"
#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include "toonz/autoclose.h"
#include "tenv.h"
#include "tropcm.h"

#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"

#include <stack>

#include <QDebug>

extern TEnv::DoubleVar AutocloseDistance;
extern TEnv::DoubleVar AutocloseAngle;
extern TEnv::IntVar AutocloseInk;
extern TEnv::IntVar AutocloseOpacity;

//-----------------------------------------------------------------------------
namespace {  // Utility Function
//-----------------------------------------------------------------------------

inline TPoint nearestInkNotDiagonal(const TRasterCM32P &r, const TPoint &p) {
  TPixelCM32 *buf = (TPixelCM32 *)r->pixels(p.y) + p.x;

  if (p.x < r->getLx() - 1 && (!(buf + 1)->isPurePaint()))
    return TPoint(p.x + 1, p.y);

  if (p.x > 0 && (!(buf - 1)->isPurePaint())) return TPoint(p.x - 1, p.y);

  if (p.y < r->getLy() - 1 && (!(buf + r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y + 1);

  if (p.y > 0 && (!(buf - r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y - 1);

  return TPoint(-1, -1);
}

//
// from point x, y expands to the right and left.
// the redrawn line goes from* xa to* xb inclusive
// x1 <= *xa <= *xb <= x2
// N.B. if not even one pixel is drawn* xa > * xb
//
// "prevailing" is set to false on revert-filling the border of
// region in the Rectangular, Freehand and Polyline fill procedures
// in order to make the paint extend behind the line.

// Calculates the endpoints for the line of pixels in which to fill
bool calcFillRow(const TRasterCM32P &r, const TPoint &p, int &xa, int &xb,
                 int paint, TPalette *palette, bool prevailing = true,
                 bool emptyOnly = false) {
  int tone, oldtone;
  TPixelCM32 *pix, *pix0, *limit, *tmp_limit;

  /* vai a destra */
  TPixelCM32 *line = r->pixels(p.y);

  pix0    = line + p.x;
  pix     = pix0;
  limit   = line + r->getBounds().x1;
  oldtone = pix->getTone();
  tone    = oldtone;
  std::cout << "calcFillRow xa:";
  std::cout << xa;
  std::cout << " xb:";
  std::cout << xb;
  for (; pix <= limit; pix++) {
    std::cout << " x:";
    std::cout << p.x + pix - pix0;
    if (pix->getPaint() == paint) {
        std::cout << " r_eq_paint:";
        std::cout << paint;
        std::cout << "_x:";
        std::cout << p.x + pix - pix0;
        std::cout << ":";
        std::cout << pix->getInk();
        std::cout << ".";
        std::cout << pix->getPaint();
        std::cout << ".";
        std::cout << pix->getTone();
        //break;
    }
    if (emptyOnly && pix->getPaint() != 0) {
        std::cout << " r_(emptyOnly&&pix->getPaint()!=0)_x:";
        std::cout << p.x + pix - pix0;
        std::cout << ":";
        std::cout << pix->getInk();
        std::cout << ".";
        std::cout << pix->getPaint();
        std::cout << ".";
        std::cout << pix->getTone();
        //break;
    }
    tone = pix->getTone();
    if (tone == 0) {
        std::cout << " r_(tone==0)_x";
        std::cout << p.x + pix - pix0;
        std::cout << ":";
        std::cout << pix->getInk();
        std::cout << ".";
        std::cout << pix->getPaint();
        std::cout << ".";
        std::cout << pix->getTone();
        break;
    }
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
      std::cout << " r_tone>oldtone_x:";
      std::cout << p.x + pix - pix0;
      std::cout << ":";
      std::cout << pix->getInk();
      std::cout << ".";
      std::cout << pix->getPaint();
      std::cout << ".";
      std::cout << pix->getTone();
      // not-yet-colored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        std::cout << " prevailing_or_notPurePaint_or_inkNotEqPaint_x:";
        std::cout << p.x + pix - pix0;
        break;
      while (pix != pix0) {
        // iterate back in order to leave the pixel with the lowest tone
        // unpainted
        pix--;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone())
                std::cout << " l_getTone()>oldtone_continue";
              continue;
          }
          std::cout << " r_getTone()>oldtone_x";
          std::cout << p.x + pix - pix0;
          std::cout << ":";
          std::cout << pix->getInk();
          std::cout << ".";
          std::cout << pix->getPaint();
          std::cout << ".";
          std::cout << pix->getTone();
          break;
        }
      }
      pix++;
      break;
    }
    oldtone = tone;
  }
  if (tone == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      //if (pix->getPaint() == paint) break; // commented out for issue 1151
      if (pix->getTone() != 0) {
          std::cout << " r_(tone!=0)_x";
          std::cout << p.x + pix - pix0;
          std::cout << ":";
          std::cout << pix->getInk();
          std::cout << ".";
          std::cout << pix->getPaint();
          std::cout << ".";
          std::cout << pix->getTone();
          break;
      }
    }
  }

  xb = p.x + pix - pix0 - 1; //go backward one pixel from the current pixel which triggered the boundary condition.

  /* go left */

  pix     = pix0;
  limit   = line + r->getBounds().x0;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (pix--; pix >= limit; pix--) {
      if (pix->getPaint() == paint) { 
          std::cout << " l_eq_paint:";
          std::cout << paint;
          std::cout << "_x:";
          std::cout << p.x + pix - pix0;
          std::cout << ":";
          std::cout << pix->getInk();
          std::cout << ".";
          std::cout << pix->getPaint();
          std::cout << ".";
          std::cout << pix->getTone();
          break; 
      }
      if (emptyOnly && pix->getPaint() != 0) {
          std::cout << " l_getPaint!=0_x:";
          std::cout << p.x + pix - pix0;
          std::cout << ":";
          std::cout << pix->getInk();
          std::cout << ".";
          std::cout << pix->getPaint();
          std::cout << ".";
          std::cout << pix->getTone();
          break; 
      }
    tone = pix->getTone();
    if (tone == 0) {
        std::cout << " l_(tone==0)_x";
        std::cout << p.x + pix - pix0;
        std::cout << ":";
        std::cout << pix->getInk();
        std::cout << ".";
        std::cout << pix->getPaint();
        std::cout << ".";
        std::cout << pix->getTone();
        break;
    }
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
        std::cout << " l_tone>oldtone_x:";
        std::cout << p.x + pix - pix0;
        std::cout << ":";
        std::cout << pix->getInk();
        std::cout << ".";
        std::cout << pix->getPaint();
        std::cout << ".";
        std::cout << pix->getTone();
      // not-yet-colored line case
        if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint()){
            std::cout << " prevailing_notPurePaint_or_pix_ink!=pix_paint";
            break;
        }
      while (pix != pix0) {
        // iterate forward in order to leave the pixel with the lowest tone
        // unpainted
        pix++;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone()) {
                std::cout << " l_getTone()>oldtone_continue";
                continue;
            }
          }
          std::cout << " l_getTone()>oldtone_x";
          std::cout << p.x + pix - pix0;
          std::cout << ":";
          std::cout << pix->getInk();
          std::cout << ".";
          std::cout << pix->getPaint();
          std::cout << ".";
          std::cout << pix->getTone();
          break;
        }
      }
      pix--;
      break;
    }
    oldtone = tone;
  }
  if (tone == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      //if (pix->getPaint() == paint) break; // commented out for issue 1151
        if (pix->getTone() != 0) { 
            std::cout << " l_(tone!=0) x";
            std::cout << p.x + pix - pix0;
            std::cout << ":";
            std::cout << pix->getInk();
            std::cout << ".";
            std::cout << pix->getPaint();
            std::cout << ".";
            std::cout << pix->getTone();
            break; 
        }
    }
  }

  xa = p.x + pix - pix0 + 1; //go backward one pixel from the current pixel which triggered the boundary condition.

  return (xb >= xa);
}

//-----------------------------------------------------------------------------

void findSegment(const TRaster32P &r, const TPoint &p, int &xa, int &xb,
                 const TPixel32 &color, const int fillDepth = 254) {
  int matte, oldmatte;
  TPixel32 *pix, *pix0, *limit, *tmp_limit;

  std::cout << "\nanonymous-namespace.findSegment ";
  /* vai a destra */
  TPixel32 *line = r->pixels(p.y);

  pix0     = line + p.x;
  pix      = pix0;
  limit    = line + r->getBounds().x1;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix <= limit; pix++) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xb = p.x + pix - pix0 - 1;

  /* vai a sinistra */
  pix      = pix0;
  limit    = line + r->getBounds().x0;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix >= limit; pix--) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xa = p.x + pix - pix0 + 1;

  std::cout << " xa:";
  std::cout << xa;
  std::cout << " xb:";
  std::cout << xb;
}

//-----------------------------------------------------------------------------
// Used when the clicked pixel is solid or semi-transparent.
// Check if the fill is stemmed at the target pixel.
// Note that RGB values are used for checking the difference, not Alpha value.

bool doesStemFill(const TPixel32 &clickColor, const TPixel32 *targetPix,
                  const int fillDepth2) {
  // stop if the target pixel is transparent
  if (targetPix->m == 0) return true;
  // check difference of RGB values is larger than fillDepth
  int dr = (int)clickColor.r - (int)targetPix->r;
  int dg = (int)clickColor.g - (int)targetPix->g;
  int db = (int)clickColor.b - (int)targetPix->b;
  return (dr * dr + dg * dg + db * db) >
         fillDepth2;  // condition for "stem" the fill
}

//-----------------------------------------------------------------------------

void fullColorFindSegment(const TRaster32P &r, const TPoint &p, int &xa,
                          int &xb, const TPixel32 &color,
                          const TPixel32 &clickedPosColor,
                          const int fillDepth) {
  if (clickedPosColor.m == 0) {
    findSegment(r, p, xa, xb, color, fillDepth);
    return;
  }

  TPixel32 *pix, *pix0, *limit;
  // check to the right
  TPixel32 *line = r->pixels(p.y);

  pix0  = line + p.x;  // seed pixel
  pix   = pix0;
  limit = line + r->getBounds().x1;  // right end

  TPixel32 oldPix = *pix;

  int fillDepth2 = fillDepth * fillDepth;

  for (; pix <= limit; pix++) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xb = p.x + pix - pix0 - 1;

  // check to the left
  pix    = pix0;                      // seed pixel
  limit  = line + r->getBounds().x0;  // left end
  oldPix = *pix;
  for (; pix >= limit; pix--) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xa = p.x + pix - pix0 + 1;
}

//-----------------------------------------------------------------------------

bool calcRefFillRow(TRaster32P &refRas, const TPoint &p, int &xa, int &xb,
                    const TPixel32 &color, const TPixel32 &clickedPosColor,
                    const int fillDepth = 254) {
  fullColorFindSegment(refRas, p, xa, xb, color, clickedPosColor, fillDepth);

  return (xb >= xa);
}

//-----------------------------------------------------------------------------

class FillSeed {
public:
  int m_xa, m_xb;
  int m_y, m_dy;
  FillSeed(int xa, int xb, int y, int dy)
      : m_xa(xa), m_xb(xb), m_y(y), m_dy(dy) {}
};

//-----------------------------------------------------------------------------

inline int threshTone(const TPixelCM32 &pix, int fillDepth) {
  if (fillDepth == TPixelCM32::getMaxTone())
    return pix.getTone();
  else
    return ((pix.getTone()) > fillDepth) ? TPixelCM32::getMaxTone()
                                         : pix.getTone();
}

void fillRow(const TRasterCM32P &r, const TPoint &p, int xa, int xb, int paint,
             TPalette *palette, TTileSaverCM32 *saver) {
    std::cout << " paint_4:";
    std::cout << paint;
  /* vai a destra */
  TPixelCM32 *line = r->pixels(p.y);
  TPixelCM32 *pix  = line + p.x;

  if (saver) saver->save(TRect(xa, p.y, xb, p.y));

  if (xb >= xa) {
    pix = line + xa;
    int n;
    for (n = 0; n < xb - xa + 1; n++, pix++) {
      if (palette && pix->isPurePaint()) {
        TPoint pInk = nearestInkNotDiagonal(r, TPoint(xa + n, p.y));
        if (pInk != TPoint(-1, -1)) {
          TPixelCM32 *pixInk =
              (TPixelCM32 *)r->getRawData() + (pInk.y * r->getWrap() + pInk.x);
          if (pixInk->getInk() != paint &&
              palette->getStyle(pixInk->getInk())->getFlags() != 0)
            inkFill(r, pInk, paint, 0, saver);
        }
      }

      pix->setPaint(paint);
    }
  }
}

//-----------------------------------------------------------------------------

inline int threshMatte(int matte, int fillDepth) {
  if (fillDepth == 255)
    return matte;
  else
    return (matte < fillDepth) ? 255 : matte;
}

//-----------------------------------------------------------------------------

bool isPixelInSegment(const std::vector<std::pair<int, int>> &segments, int x) {
  for (int i = 0; i < (int)segments.size(); i++) {
    std::pair<int, int> segment = segments[i];
    if (segment.first <= x && x <= segment.second) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void insertSegment(std::vector<std::pair<int, int>> &segments,
                   const std::pair<int, int> segment) {
  for (int i = segments.size() - 1; i >= 0; i--) {
    std::pair<int, int> app = segments[i];
    if (segment.first <= app.first && app.second <= segment.second)
      segments.erase(segments.begin() + i);
  }
  segments.push_back(segment);
}

//-----------------------------------------------------------------------------

bool floodCheck(const TPixel32 &clickColor, const TPixel32 *targetPix,
                const TPixel32 *oldPix, const int fillDepth) {
  auto fullColorThreshMatte = [](int matte, int fillDepth) -> int {
    return (matte <= fillDepth) ? matte : 255;
  };

  if (clickColor.m == 0) {
    int oldMatte = fullColorThreshMatte(oldPix->m, fillDepth);
    int matte    = fullColorThreshMatte(targetPix->m, fillDepth);
    return matte >= oldMatte && matte != 255;
  }
  int fillDepth2 = fillDepth * fillDepth;
  return !doesStemFill(clickColor, targetPix, fillDepth2);
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

TRasterCM32P convertRaster2CM(const TRasterP &inputRaster) {
  int lx = inputRaster->getLx();
  int ly = inputRaster->getLy();

  TRaster32P r = inputRaster;

  TRasterCM32P rout(lx, ly);

  for (int y = 0; y < ly; y++) {
    TPixel32 *pixin    = r->pixels(y);
    TPixel32 *pixinEnd = pixin + lx;
    TPixelCM32 *pixout = rout->pixels(y);
    while (pixin < pixinEnd) {
      if (*pixin == TPixel32(0, 0, 0, 0)) {
        ++pixin;
        *pixout++ = TPixelCM32(0, 0, 255);
      } else {
        int v = (pixin->r + pixin->g + pixin->b) / 3;
        ++pixin;
        *pixout++ = TPixelCM32(1, 0, v);
      }
    }
  }
  return rout;
}

/*-- The return value is whether the saveBox has been updated or not. --*/
bool fill(const TRasterCM32P &r, const FillParameters &params,
          TTileSaverCM32 *saver, bool fillGaps, bool closeGaps,
          int closeStyleIndex, double autoCloseDistance, TXsheet *xsheet,
          int frameIndex) {
  std::cout << "\nGlobalScope.fill_2";
  auto fullColorThreshMatte = [](int matte, int fillDepth) -> int {
    return (matte <= fillDepth) ? matte : 255;
  };

  TPixelCM32 *pix, *limit, *pix0, *oldpix;
  TPixel32 *refpix, *oldrefpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int tone, oldtone;
  TPoint p = params.m_p;
  int x = p.x, y = p.y;
  int paint = params.m_styleId;
  std::cout << " paint_1=:";
  std::cout << paint;
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
  TRasterCM32P tempRaster;
  int styleIndex                                 = 4094;
  int fakeStyleIndex                             = 4095;
  if (autoCloseDistance < 0.0) autoCloseDistance = AutocloseDistance;

  bool gapsClosed = false, refGapsClosed = false;

  if (fillGaps) {
    tempRaster = r->clone();
    gapsClosed = TAutocloser(tempRaster, autoCloseDistance, AutocloseAngle,
                             styleIndex, AutocloseOpacity)
                     .exec();
  }
  if (!gapsClosed) {
    tempRaster = r;
  }

  /*-- getBounds returns the entire image --*/
  TRect bbbox = tempRaster->getBounds();

  /*- Return if clicked outside the screen -*/
  if (!bbbox.contains(p)) return false;
  /*- If the same color has already been painted, return -*/
  int paintAtClickedPos = (tempRaster->pixels(p.y) + p.x)->getPaint();
  if (paintAtClickedPos == paint) return false;
  /*- If the "paint only transparent areas" option is enabled and the area is
   * already colored, return
   * -*/
  if (params.m_emptyOnly && (tempRaster->pixels(p.y) + p.x)->getPaint() != 0)
    return false;

  TRaster32P refRaster(bbbox.getSize());
  TPixel32 clickedPosColor, color(255, 255, 255);

  std::map<int, std::vector<std::pair<int, int>>> segments;

  if (xsheet) {
    ToonzScene *scene = xsheet->getScene();
    TCamera *camera   = scene->getCurrentCamera();
    TRaster32P tmpRaster(camera->getRes());

    // Render for reference
    scene->renderFrame(tmpRaster, frameIndex, xsheet, false, false, true);

    refRaster->lock();

    TPoint offset((params.m_imageSize.lx - tmpRaster->getLx()) / 2,
                  (params.m_imageSize.ly - tmpRaster->getLy()) / 2);
    offset -= params.m_imageOffset;

    refRaster->fill(color);
    refRaster->copy(tmpRaster, offset);

    refpix          = refRaster->pixels(p.y) + p.x;
    clickedPosColor = *refpix;

    if (params.m_emptyOnly && clickedPosColor != TPixel32(0, 0, 0, 0)) {
      refRaster->unlock();
      return false;
    }

    if (fillGaps) {
      TRasterCM32P cr          = convertRaster2CM(refRaster);
      TRasterCM32P refCMRaster = cr->clone();

      refGapsClosed = TAutocloser(refCMRaster, autoCloseDistance,
                                  AutocloseAngle, styleIndex, AutocloseOpacity)
                          .exec();
      if (refGapsClosed) {
        if (!gapsClosed) tempRaster = r->clone();

        // Transfer the gap segments to the refRaster
        TPixelCM32 *tempPix  = tempRaster->pixels(0);
        TPixelCM32 *refCMPix = refCMRaster->pixels(0);
        TPixel32 *refPix     = refRaster->pixels(0);
        for (int refCMY = 0; refCMY < refCMRaster->getLy(); refCMY++) {
          for (int refCMX = 0; refCMX < refCMRaster->getLx();
               refCMX++, refCMPix++, refPix++, tempPix++) {
            if (refCMPix->getInk() != styleIndex) continue;
            *refPix = color;
            if (closeGaps) {
              tempPix->setInk(refCMPix->getInk());
              tempPix->setTone(refCMPix->getTone());
            }
          }
        }
      }
    }
  }

  if (fillGaps && !gapsClosed && !refGapsClosed) fillGaps = false;

  assert(fillDepth >= 0 && fillDepth < 16);

  if (xsheet)
    fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);
  else
    switch (TPixelCM32::getMaxTone()) {
    case 15:
      fillDepth = (15 - fillDepth);
      break;
    case 255:
      fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);
      break;
    default:
      assert(false);
    }
  /*--Look at the colors in the four corners and update the saveBox if any of
   * the colors change. --*/
  TPixelCM32 borderIndex[4];
  TPixelCM32 *borderPix[4];
  pix            = tempRaster->pixels(0);
  borderPix[0]   = pix;
  borderIndex[0] = *pix;
  pix += tempRaster->getLx() - 1;
  borderPix[1]   = pix;
  borderIndex[1] = *pix;
  pix            = tempRaster->pixels(tempRaster->getLy() - 1);
  borderPix[2]   = pix;
  borderIndex[2] = *pix;
  pix += tempRaster->getLx() - 1;
  borderPix[3]   = pix;
  borderIndex[3] = *pix;

  std::stack<FillSeed> seeds;

  std::cout << "\ny:";
  std::cout << y;
  std::cout << " ";
  
  std::cout << " paint_2=:";
  std::cout << paint;

  bool fillIt =
      !xsheet ? calcFillRow(tempRaster, p, xa, xb, paint, params.m_palette,
                            params.m_prevailing, params.m_emptyOnly)
              : calcRefFillRow(refRaster, p, xa, xb, color, clickedPosColor,
                               fillDepth);
  std::cout << " paint_3=:";
  std::cout << paint;

  if (fillIt) fillRow(tempRaster, p, xa, xb, paint, params.m_palette, saver);
  if (xsheet) segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

    // Start: Set the ink on gaps that were used to their final value, NOTE: This is duplicate code, 1 of 2  
  if (fillGaps || closeGaps) {
      std::cout << "\nY:";
      std::cout << y;
      std::cout << " xa:";
      std::cout << xa;
      std::cout << " xb:";
      std::cout << xb;

    TPixelCM32 *tempPix = tempRaster->pixels(0);
    tempPix += (y * tempRaster->getLx()) + xa - 1;
    int i = xa;
    // std::cout << "\n1:";
    // std::cout << y;
    // std::cout << ":i:";
    // std::cout << i;
    // std::cout << ":xb:";
    // std::cout << xb;

    while (i <= xb) {
       std::cout << " a1:";
       std::cout << i;
       std::cout << ":";
       std::cout << tempPix->getInk();
       std::cout << ".";
       std::cout << tempPix->getPaint();
       std::cout << ".";
       std::cout << tempPix->getTone();

      // if (tempPix->getInk() == styleIndex) {
      //  tempPix->setInk(fakeStyleIndex);
      //  //std::cout << " *4095* ";
      //}

      if (tempPix->getPaint() == paint) {
        // check for neighboring gap pixels and finalize them
        // west
        if ((tempPix - 1)->getInk() == styleIndex) {
          if (closeGaps) {
            // keep as ink pixel
            (tempPix - 1)->setInk(closeStyleIndex);
            (tempPix - 1)->setPaint(paint);
            (tempPix - 1)->setTone(0);
          } else {
            // keep as paint pixel
            (tempPix - 1)->setInk(0);
            (tempPix - 1)->setPaint(paint);
            (tempPix - 1)->setTone(255);
          }
        }
        // east
        if ((tempPix + 1)->getInk() == styleIndex) {
          if (closeGaps) {
            // keep as ink pixel
            (tempPix + 1)->setInk(closeStyleIndex);
            (tempPix + 1)->setPaint(paint);
            (tempPix + 1)->setTone(0);
          } else {
            // keep as paint pixel
            (tempPix + 1)->setInk(0);
            (tempPix + 1)->setPaint(paint);
            (tempPix + 1)->setTone(255);
          }
        }
        // north
        if ((tempPix + tempRaster->getWrap())->getInk() == styleIndex) {
          if (closeGaps) {
            // keep as ink pixel
            (tempPix + tempRaster->getWrap())->setInk(closeStyleIndex);
            (tempPix + tempRaster->getWrap())->setPaint(paint);
            (tempPix + tempRaster->getWrap())->setTone(0);
          } else {
            // keep as paint pixel
            (tempPix + tempRaster->getWrap())->setInk(0);
            (tempPix + tempRaster->getWrap())->setPaint(paint);
            (tempPix + tempRaster->getWrap())->setTone(255);
          }
        }
        // south
        if ((tempPix - tempRaster->getWrap())->getInk() == styleIndex) {
          if (closeGaps) {
            // keep as ink pixel
            (tempPix - tempRaster->getWrap())->setInk(closeStyleIndex);
            (tempPix - tempRaster->getWrap())->setPaint(paint);
            (tempPix - tempRaster->getWrap())->setTone(0);
          } else {
            // keep as paint pixel
            (tempPix - tempRaster->getWrap())->setInk(0);
            (tempPix - tempRaster->getWrap())->setPaint(paint);
            (tempPix - tempRaster->getWrap())->setTone(255);
          }
        }
      }

       std::cout << " b1:";
       std::cout << i;
       std::cout << ":";
       std::cout << tempPix->getInk();
       std::cout << ".";
       std::cout << tempPix->getPaint();
       std::cout << ".";
       std::cout << tempPix->getTone();
      tempPix++;
      i++;
    }
  }

std::cout << "\nFill-Seeds";

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    pix = pix0 = tempRaster->pixels(y) + xa;
    limit      = tempRaster->pixels(y) + xb;
    oldpix     = tempRaster->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();

    if (xsheet) {
      refpix    = refRaster->pixels(y) + xa;
      oldrefpix = refRaster->pixels(oldy) + xa;
    }
    while (pix <= limit) {
      bool canPaint = false;
      if (!xsheet) {
        oldtone = threshTone(*oldpix, fillDepth);
        tone    = threshTone(*pix, fillDepth);
        // the last condition is added in order to prevent fill area from
        // protruding behind the colored line
        canPaint = pix->getPaint() != paint && tone <= oldtone &&
                   (!params.m_emptyOnly || pix->getPaint() == 0) && tone != 0 &&
                   (pix->getPaint() != pix->getInk() ||
                    pix->getPaint() == paintAtClickedPos);
      } else {
        bool test = false;
        if (segments.find(y) != segments.end())
          test   = isPixelInSegment(segments[y], x);
        canPaint = *refpix != color && !test &&
                   floodCheck(clickedPosColor, refpix, oldrefpix, fillDepth);
      }
      if (canPaint) {
        std::cout << "\ny:";
        std::cout << y;
        std::cout << " ";
        bool fillIt = !xsheet
                          ? calcFillRow(tempRaster, TPoint(x, y), xc, xd, paint,
                                        params.m_palette, params.m_prevailing)
                          : calcRefFillRow(refRaster, TPoint(x, y), xc, xd,
                                           color, clickedPosColor, fillDepth);
        if (fillIt)
          fillRow(tempRaster, TPoint(x, y), xc, xd, paint, params.m_palette,
                  saver);
        if (xsheet) insertSegment(segments[y], std::pair<int, int>(xc, xd));
        
        // Set the ink on gaps that were used to 4095
        // 
        //{
        //  TPixelCM32 *tempPix = tempRaster->pixels(0);
        //  tempPix += (y * tempRaster->getLx()) + xa - 1;
        //  int i = xa;
        //  //std::cout << "\n2:";
        //  //std::cout << y;
        //  //std::cout << ":i:";
        //  //std::cout << i;
        //  //std::cout << ":xb:";
        //  //std::cout << xb;

        //  while (i <= xb) {
        //    //std::cout << " a2:";
        //    //std::cout << i;
        //    //std::cout << ":";
        //    //std::cout << tempPix->getInk();
        //    //std::cout << ".";
        //    //std::cout << tempPix->getPaint();
        //    //std::cout << ".";
        //    //std::cout << tempPix->getTone();

        //    if (tempPix->getInk() == styleIndex) {
        //      tempPix->setInk(fakeStyleIndex);
        //      //std::cout << " *4095* ";
        //    }
        //    //// check the neighboring pixels for gap close ink
        //    //// west
        //    //if (i == xa) {
        //    //  if ((tempPix - 1)->getInk() == styleIndex) {
        //    //    (tempPix - 1)->setInk(fakeStyleIndex);
        //    //    //std::cout << " *4095 w* ";
        //    //  }
        //    //}
        //    //// east
        //    //if (i == xb) {
        //    //  if ((tempPix + 1)->getInk() == styleIndex) {
        //    //    (tempPix + 1)->setInk(fakeStyleIndex);
        //    //    //std::cout << " *4095 e* ";
        //    //  }
        //    //}
        //    //// north
        //    //if ((tempPix + tempRaster->getWrap())->getInk() == styleIndex) {
        //    //  (tempPix + tempRaster->getWrap())->setInk(fakeStyleIndex);
        //    //  //std::cout << " *4095 n* ";
        //    //}
        //    //// south
        //    //if ((tempPix - tempRaster->getWrap())->getInk() == styleIndex) {
        //    //  (tempPix - tempRaster->getWrap())->setInk(fakeStyleIndex);
        //    //  //std::cout << " *4095 s* ";
        //    //}
        //    //std::cout << " b2:";
        //    //std::cout << i;
        //    //std::cout << ":";
        //    //std::cout << tempPix->getInk();
        //    //std::cout << ".";
        //    //std::cout << tempPix->getPaint();
        //    //std::cout << ".";
        //    //std::cout << tempPix->getTone();

        //    tempPix++;
        //    i++;
        //  }
        //}

        // Start: Set the ink on gaps that were used to their final value, NOTE: This is duplicate code, 2 of 2
        if (fillGaps || closeGaps) {
            std::cout << "\nY:";
            std::cout << y;
            std::cout << " xa:";
            std::cout << xa;
            std::cout << " xb:";
            std::cout << xb;

          TPixelCM32 *tempPix = tempRaster->pixels(0);
          tempPix += (y * tempRaster->getLx()) + xa - 1;
          int i = xa;
          // std::cout << "\n1:";
          // std::cout << y;
          // std::cout << ":i:";
          // std::cout << i;
          // std::cout << ":xb:";
          // std::cout << xb;

          while (i <= xb) {
             std::cout << " a2:";
             std::cout << i;
             std::cout << ":";
             std::cout << tempPix->getInk();
             std::cout << ".";
             std::cout << tempPix->getPaint();
             std::cout << ".";
             std::cout << tempPix->getTone();

            // if (tempPix->getInk() == styleIndex) {
            //  tempPix->setInk(fakeStyleIndex);
            //  //std::cout << " *4095* ";
            //}

            if (tempPix->getPaint() == paint) {
              // check for neighboring gap pixels and finalize them
              // west
              if ((tempPix - 1)->getInk() == styleIndex) {
                (tempPix - 1)->setInk(fakeStyleIndex);
                //if (closeGaps) {
                //  // keep as ink pixel
                //  (tempPix - 1)->setInk(closeStyleIndex);
                //  (tempPix - 1)->setPaint(paint);
                //  (tempPix - 1)->setTone(0);
                //} else {
                //  // keep as paint pixel
                //  (tempPix - 1)->setInk(0);
                //  (tempPix - 1)->setPaint(paint);
                //  (tempPix - 1)->setTone(255);
                //}
              }
              // east
              if ((tempPix + 1)->getInk() == styleIndex) {
                  (tempPix + 1)->setInk(fakeStyleIndex);
                //if (closeGaps) {
                //  // keep as ink pixel
                //  (tempPix + 1)->setInk(closeStyleIndex);
                //  (tempPix + 1)->setPaint(paint);
                //  (tempPix + 1)->setTone(0);
                //} else {
                //  // keep as paint pixel
                //  (tempPix + 1)->setInk(0);
                //  (tempPix + 1)->setPaint(paint);
                //  (tempPix + 1)->setTone(255);
                //}
              }
              // north
              if ((tempPix + tempRaster->getWrap())->getInk() == styleIndex) {
                (tempPix + tempRaster->getWrap())->setInk(fakeStyleIndex);
                //if (closeGaps) {
                //  // keep as ink pixel
                //  (tempPix + tempRaster->getWrap())->setInk(closeStyleIndex);
                //  (tempPix + tempRaster->getWrap())->setPaint(paint);
                //  (tempPix + tempRaster->getWrap())->setTone(0);
                //} else {
                //  // keep as paint pixel
                //  (tempPix + tempRaster->getWrap())->setInk(0);
                //  (tempPix + tempRaster->getWrap())->setPaint(paint);
                //  (tempPix + tempRaster->getWrap())->setTone(255);
                //}
              }
              // south
              if ((tempPix - tempRaster->getWrap())->getInk() == styleIndex) {
                (tempPix - tempRaster->getWrap())->setInk(fakeStyleIndex);
                //if (closeGaps) {
                //  // keep as ink pixel
                //  (tempPix - tempRaster->getWrap())->setInk(closeStyleIndex);
                //  (tempPix - tempRaster->getWrap())->setPaint(paint);
                //  (tempPix - tempRaster->getWrap())->setTone(0);
                //} else {
                //  // keep as paint pixel
                //  (tempPix - tempRaster->getWrap())->setInk(0);
                //  (tempPix - tempRaster->getWrap())->setPaint(paint);
                //  (tempPix - tempRaster->getWrap())->setTone(255);
                //}
              }
            }

             std::cout << " b2:";
             std::cout << i;
             std::cout << ":";
             std::cout << tempPix->getInk();
             std::cout << ".";
             std::cout << tempPix->getPaint();
             std::cout << ".";
             std::cout << tempPix->getTone();
            tempPix++;
            i++;
          }
        }// End: Set the ink on gaps that were used to their final value


        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        if (xsheet) {
          refpix += xd - x + 1;
          oldrefpix += xd - x + 1;
        }
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
        if (xsheet) {
          refpix++;
          oldrefpix++;
        }
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  bool saveBoxChanged = false;

  if (xsheet) {
    refRaster->unlock();
    saveBoxChanged = true;
  } else {
    for (int i = 0; i < 4; i++) {
      if (!((*borderPix[i]) == borderIndex[i])) {
        saveBoxChanged = true;
        break;
      }
    }
  }

  if (fillGaps) {
    TPixelCM32 *tempPix = tempRaster->pixels();
    TPixelCM32 *keepPix = r->pixels();
    std::cout << "\nfill_final_check----y:";
    std::cout << tempRaster->getLy();
    std::cout << ",x:";
    std::cout << tempRaster->getLx();
    std::cout << "----";
    for (int tempY = 0; tempY < tempRaster->getLy(); tempY++) {
      //std::cout << "\n";
      //std::cout << tempY;
      for (int tempX = 0; tempX < tempRaster->getLx();
           tempX++, tempPix++, keepPix++) {
         //std::cout << " a";
         //std::cout << tempX;
         //std::cout << ":";
         //std::cout << tempPix->getInk();
         //std::cout << ".";
         //std::cout << tempPix->getPaint();
         //std::cout << ".";
         //std::cout << tempPix->getTone();
         //std::cout << ":";
         //std::cout << keepPix->getInk();
         //std::cout << ".";
         //std::cout << keepPix->getPaint();
         //std::cout << ".";
         //std::cout << keepPix->getTone();

        // if (tempPix->getInk() != styleIndex &&
        //    tempPix->getInk() != fakeStyleIndex) {
        // std::cout << "\ntempY:";
        // std::cout << tempY;
        // std::cout << "\ttempX:";
        // std::cout << tempX;
        // std::cout << "\ttemPix.getInk():";
        // std::cout << tempPix->getInk();
        // std::cout << "\ttemPix.getPaint():";
        // std::cout << tempPix->getPaint();
        // keepPix->setPaint(tempPix->getPaint());
        //}
        // Handle pixels of gap close lines, 4094, 4095

        if (tempPix->getInk() == fakeStyleIndex ||
            tempPix->getInk() == styleIndex) {
            if (tempPix->getInk() == fakeStyleIndex) {
               if(closeGaps){ //keep as ink pixel
                    keepPix->setInk(closeStyleIndex);
                    keepPix->setPaint(paint);
                    keepPix->setTone(0);
                } else { //keep as paint pixel
                    keepPix->setInk(0);
                    keepPix->setPaint(paint);
                    keepPix->setTone(255);
                }
            }else{
                // an unused close gap pixel, so ignore
            }
        //  if (fillGaps || closeGaps) {
        //        

        //    std::cout << "\nNorth "; //north
        //    std::cout << tempY+1;
        //    std::cout << "  ";
        //    std::cout << tempX;
        //    std::cout << ":";
        //    std::cout << (tempPix + tempRaster->getWrap())->getInk();
        //    std::cout << ".";
        //    std::cout << (tempPix + tempRaster->getWrap())->getPaint();
        //    std::cout << ".";
        //    std::cout << (tempPix + tempRaster->getWrap())->getTone();
        //    std::cout << ";";
        //    std::cout << (keepPix + r->getWrap())->getInk();
        //    std::cout << ".";
        //    std::cout << (keepPix + r->getWrap())->getPaint();
        //    std::cout << ".";
        //    std::cout << (keepPix + r->getWrap())->getTone();

        //    std::cout << "\nWest:Center:East "; //west, center, east
        //    std::cout << tempY;
        //    std::cout << " ";
        //    // west
        //    std::cout << tempX-1;
        //    std::cout << ":";
        //    std::cout << (tempPix - 1)->getInk();
        //    std::cout << ".";
        //    std::cout << (tempPix - 1)->getPaint();
        //    std::cout << ".";
        //    std::cout << (tempPix - 1)->getTone();
        //    std::cout << ";";
        //    std::cout << (keepPix - 1)->getInk();
        //    std::cout << ".";
        //    std::cout << (keepPix - 1)->getPaint();
        //    std::cout << ".";
        //    std::cout << (keepPix - 1)->getTone();
        //    // center
        //    std::cout << " ";
        //    std::cout << tempX;
        //    std::cout << ":";
        //    std::cout << tempPix->getInk();
        //    std::cout << ".";
        //    std::cout << tempPix->getPaint();
        //    std::cout << ".";
        //    std::cout << tempPix->getTone();
        //    std::cout << ";";
        //    std::cout << keepPix->getInk();
        //    std::cout << ".";
        //    std::cout << keepPix->getPaint();
        //    std::cout << ".";
        //    std::cout << keepPix->getTone();
        //    // east
        //    std::cout << " ";
        //    std::cout << tempX+1;
        //    std::cout << ":";
        //    std::cout << (tempPix + 1)->getInk();
        //    std::cout << ".";
        //    std::cout << (tempPix + 1)->getPaint();
        //    std::cout << ".";
        //    std::cout << (tempPix + 1)->getTone();
        //    std::cout << ";";
        //    std::cout << (keepPix + 1)->getInk();
        //    std::cout << ".";
        //    std::cout << (keepPix + 1)->getPaint();
        //    std::cout << ".";
        //    std::cout << (keepPix + 1)->getTone();
        //    // south
        //    std::cout << "\nSouth ";
        //    std::cout << tempY-1;
        //    std::cout << "  ";
        //    std::cout << tempX;
        //    std::cout << ":";
        //    std::cout << (tempPix - tempRaster->getWrap())->getInk();
        //    std::cout << ".";
        //    std::cout << (tempPix - tempRaster->getWrap())->getPaint();
        //    std::cout << ".";
        //    std::cout << (tempPix - tempRaster->getWrap())->getTone();
        //    std::cout << ";";
        //    std::cout << (keepPix - r->getWrap())->getInk();
        //    std::cout << ".";
        //    std::cout << (keepPix - r->getWrap())->getPaint();
        //    std::cout << ".";
        //    std::cout << (keepPix - r->getWrap())->getTone();


        //    // does this pixel have a fill pixel neighbor that is new?
        //    if (
        //        ((tempX > 0) && ((tempPix - 1)->getPaint() == paint) && (tempPix - 1)->isPurePaint() && !(((keepPix - 1)->getPaint() == paint) && (keepPix - 1)->isPurePaint()) )  // west
        //        || ((tempX < tempRaster->getLx()) && ((tempPix + 1)->getPaint() == paint) && (tempPix + 1)->isPurePaint() && !(((keepPix + 1)->getPaint() == paint) && (keepPix + 1)->isPurePaint()) )  // east
        //        || ((tempPix + tempRaster->getWrap())->getPaint() == paint && (tempPix + tempRaster->getWrap())->isPurePaint() && !((keepPix + r->getWrap())->getPaint() == paint && (keepPix + r->getWrap())->isPurePaint()) )  // north
        //        || ((tempPix - tempRaster->getWrap())->getPaint() == paint && (tempPix - tempRaster->getWrap())->isPurePaint() && !((keepPix - r->getWrap())->getPaint() == paint && (keepPix - r->getWrap())->isPurePaint()) )  // south
        //    ) {// yes, keep this pixel
        //      if (closeGaps 
        //          && (
        //              ((tempX > 0) && (tempPix - 1)->getPaint() == 0 && (tempPix - 1)->isPurePaint())  // west
        //              || ((tempX < tempRaster->getLx()) && ((tempPix + 1)->getPaint() == 0) && (tempPix + 1)->isPurePaint())  // east
        //              || ((tempPix + tempRaster->getWrap())->getPaint() == 0 && (tempPix + tempRaster->getWrap())->isPurePaint())  // north
        //              || ((tempPix - tempRaster->getWrap())->getPaint() == 0 && (tempPix - tempRaster->getWrap())->isPurePaint())  // south 
        //            )
        //      ){ //keep as ink line
        //        keepPix->setInk(closeStyleIndex);
        //        keepPix->setPaint(paint);
        //        keepPix->setTone(0);
        //      } else { //keep as paint
        //        //keepPix->setInk(paint);
        //        keepPix->setPaint(paint);
        //        keepPix->setTone(255);
        //      }
        //    }
        //  } else {
        //    // Ignore unwanted gap close pixels.
        //    // Should not reach this code as those pixels should not be
        //    // generated prior to this code without fillGaps or closeGaps set to
        //    // true.
        //  }
        } else {
          //
          // Handle all other pixels
          //
          keepPix->setInk(tempPix->getInk());
          keepPix->setPaint(tempPix->getPaint());
          keepPix->setTone(tempPix->getTone());
        }
        // This next line takes care of autopaint lines
        //if (tempPix->getInk() != styleIndex) { //commented out by Tom
        //  //if (closeGaps && (tempPix->getInk() == fakeStyleIndex)) {
        //  if (tempPix->getInk() == fakeStyleIndex) {
        //    std::cout << "\n****** A pixel with ink 4095 was found ******\n";
        //    if (closeGaps) {
        //      keepPix->setInk(closeStyleIndex);
        //      // the following two lines could set up a partial ink, partial paint pixel
        //      // keepPix->setPaint(closeStyleIndex);
        //      // keepPix->setTone(tempPix->getTone()); //commented out by Tom
        //      keepPix->setTone(0);
        //    } else {
        //      keepPix->setPaint(paint);
        //      keepPix->setTone(255);
        //    }
        //  } else {
        //    keepPix->setPaint(tempPix->getPaint());
        //    keepPix->setTone(tempPix->getTone());
        //  }
        //} else { // handle tempPix->getInk() == styleIndex
            //TPoint nearestPix = nearestInkNotDiagonal(tempRaster, TPoint(tempX, tempY));
            //std::cout << "\nfor pix y: ";
            //std::cout << tempY;
            //std::cout << " x: ";
            //std::cout << tempX;
            //std::cout << " nearest pix y: ";
            //std::cout << nearestPix.y;
            //std::cout << " x: ";
            //std::cout << nearestPix.x;
            // Is this pixel next to a pixel painted with current fill color? 
            // then keep
            //keepPix->setInk(closeStyleIndex);
            //keepPix->setTone(0);
            // else, ignore
            //keepPix->setTone(255);
        //}
        //std::cout << " b";
        //std::cout << tempX;
        //std::cout << ":";
        //std::cout << tempPix->getInk();
        //std::cout << ".";
        //std::cout << tempPix->getPaint();
        //std::cout << ".";
        //std::cout << tempPix->getTone();
        //std::cout << ":";
        //std::cout << keepPix->getInk();
        //std::cout << ".";
        //std::cout << keepPix->getPaint();
        //std::cout << ".";
        //std::cout << keepPix->getTone();
      }
    }
  }
  return saveBoxChanged;
}

//-----------------------------------------------------------------------------

void fill(const TRaster32P &ras, const TRaster32P &ref,
          const FillParameters &params, TTileSaverFullColor *saver) {
  TPixel32 *pix, *limit, *pix0, *oldpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int matte, oldMatte;
  int x = params.m_p.x, y = params.m_p.y;
  TRaster32P workRas = ref ? ref : ras;

  TRect bbbox = workRas->getBounds();

  std::cout << "\nGlobalScope.fill_1\n";

  if (!bbbox.contains(params.m_p)) return;

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);

  // looking for any  pure transparent pixel along the border; if after filling
  // that pixel will be changed,
  // it means that I filled the bg and the savebox needs to be recomputed!
  TPixel32 borderIndex;
  TPixel32 *borderPix = 0;
  pix                 = workRas->pixels(0);
  int i;
  for (i = 0; i < workRas->getLx(); i++, pix++)  // border down
    if (pix->m == 0) {
      borderIndex = *pix;
      borderPix   = pix;
      break;
    }
  if (borderPix == 0)  // not found in border down...try border up (avoid left
                       // and right borders...so unlikely)
  {
    pix = workRas->pixels(workRas->getLy() - 1);
    for (i = 0; i < workRas->getLx(); i++, pix++)  // border up
      if (pix->m == 0) {
        borderIndex = *pix;
        borderPix   = pix;
        break;
      }
  }

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  // fillRow(r, params.m_p, xa, xb, color ,saver);
  findSegment(workRas, params.m_p, xa, xb, color);
  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    pix = pix0 = workRas->pixels(y) + xa;
    limit      = workRas->pixels(y) + xb;
    oldpix     = workRas->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();
    while (pix <= limit) {
      oldMatte  = threshMatte(oldpix->m, fillDepth);
      matte     = threshMatte(pix->m, fillDepth);
      bool test = false;
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      if (*pix != color && !test && matte >= oldMatte && matte != 255) {
        findSegment(workRas, TPoint(x, y), xc, xd, color);
        // segments[y].push_back(std::pair<int,int>(xc, xd));
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line    = ras->pixels(it->first);
    TPixel32 *refLine = 0;
    TPixel32 *refPix;
    if (ref) refLine = ref->pixels(it->first);
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (ref) refPix = refLine + segment.first;
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (ref) {
            *pix = *refPix;
            refPix++;
          } else
            *pix = pix->m == 0 ? color : overPix(color, *pix);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

static void rectFill(const TRaster32P &ras, const TRect &r,
                     const TPixel32 &color) {}

//-----------------------------------------------------------------------------

static TPoint nearestInk(const TRasterCM32P &r, const TPoint &p, int ray) {
  int i, j;
  TPixelCM32 *buf = (TPixelCM32 *)r->getRawData();

  for (j = std::max(p.y - ray, 0); j <= std::min(p.y + ray, r->getLy() - 1);
       j++)
    for (i = std::max(p.x - ray, 0); i <= std::min(p.x + ray, r->getLx() - 1);
         i++)
      if (!(buf + j * r->getWrap() + i)->isPurePaint()) return TPoint(i, j);

  return TPoint(-1, -1);
}

//-----------------------------------------------------------------------------

void inkFill(const TRasterCM32P &r, const TPoint &pin, int ink, int searchRay,
             TTileSaverCM32 *saver, TRect *insideRect) {
  r->lock();
  TPixelCM32 *pixels = (TPixelCM32 *)r->getRawData();
  int oldInk;
  TPoint p = pin;

  if ((pixels + p.y * r->getWrap() + p.x)->isPurePaint() &&
      (searchRay == 0 || (p = nearestInk(r, p, searchRay)) == TPoint(-1, -1))) {
    r->unlock();
    return;
  }
  TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);

  if (pix->getInk() == ink) {
    r->unlock();
    return;
  }

  oldInk = pix->getInk();

  std::stack<TPoint> seeds;
  seeds.push(p);

  while (!seeds.empty()) {
    p = seeds.top();
    seeds.pop();
    if (!r->getBounds().contains(p)) continue;
    if (insideRect && !insideRect->contains(p)) continue;

    TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);
    if (pix->isPurePaint() || pix->getInk() != oldInk) continue;

    if (saver) saver->save(p);

    pix->setInk(ink);

    seeds.push(TPoint(p.x - 1, p.y - 1));
    seeds.push(TPoint(p.x - 1, p.y));
    seeds.push(TPoint(p.x - 1, p.y + 1));
    seeds.push(TPoint(p.x, p.y - 1));
    seeds.push(TPoint(p.x, p.y + 1));
    seeds.push(TPoint(p.x + 1, p.y - 1));
    seeds.push(TPoint(p.x + 1, p.y));
    seeds.push(TPoint(p.x + 1, p.y + 1));
  }
  r->unlock();
}

//-----------------------------------------------------------------------------

void fullColorFill(const TRaster32P &ras, const FillParameters &params,
                   TTileSaverFullColor *saver, TXsheet *xsheet, int frameIndex,
                   bool fillGaps, bool closeGaps, int closeStyleIndex,
                   double autoCloseDistance) {
  int oldy, xa, xb, xc, xd, dy, oldxd, oldxc;
  TPixel32 *pix, *limit, *pix0, *oldpix, *refpix, *oldrefpix;
  TPixelCM32 *refCMpix;
  int x = params.m_p.x, y = params.m_p.y;

  TRect bbbox = ras->getBounds();
  if (!bbbox.contains(params.m_p)) return;

  TPixel32 clickedPosColor = *(ras->pixels(y) + x);

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();

  if (clickedPosColor == color) return;

  TRaster32P refRas(bbbox.getSize());

  TPixel32 gapColor  = plt->getStyle(closeStyleIndex)->getMainColor();
  int styleIndex     = 4094;
  int fakeStyleIndex = 4095;

  if (xsheet) {
    ToonzScene *scene = xsheet->getScene();
    TCamera *camera   = scene->getCurrentCamera();
    TRaster32P tmpRaster(camera->getRes());
    scene->renderFrame(tmpRaster, frameIndex, xsheet, false, false, true);

    refRas->lock();

    TPoint offset((refRas->getLx() - tmpRaster->getLx()) / 2,
                  (refRas->getLy() - tmpRaster->getLy()) / 2);
    refRas->fill(color);
    refRas->copy(tmpRaster, offset);

    clickedPosColor = *(refRas->pixels(y) + x);
  } else if (fillGaps) {
    refRas->lock();
    TPoint offset((refRas->getLx() - ras->getLx()) / 2,
                  (refRas->getLy() - ras->getLy()) / 2);
    refRas->fill(color);
    refRas->copy(ras, offset);
  }

  TRasterCM32P refCMRaster;

  if (fillGaps) {
    TRasterCM32P cr = convertRaster2CM(refRas);
    refCMRaster     = cr->clone();
    fillGaps = TAutocloser(refCMRaster, autoCloseDistance, AutocloseAngle,
                           styleIndex, AutocloseOpacity)
                   .exec();
    if (fillGaps) {
      // Transfer the gap segments to the refRaster
      refCMpix         = refCMRaster->pixels(0);
      TPixel32 *refPix = refRas->pixels(0);
      for (int refCMY = 0; refCMY < refCMRaster->getLy(); refCMY++) {
        for (int refCMX = 0; refCMX < refCMRaster->getLx();
             refCMX++, refCMpix++, refPix++) {
          if (refCMpix->getInk() != styleIndex) continue;
          *refPix = color;
        }
      }
    }
  }

  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  TPointD m_firstPoint, m_clickPoint;

  // convert fillDepth range from [0 - 15] to [0 - 255]
  fillDepth = (fillDepth << 4) | fillDepth;

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  if (!xsheet && !fillGaps)
    fullColorFindSegment(ras, params.m_p, xa, xb, color, clickedPosColor,
                         fillDepth);
  else
    fullColorFindSegment(refRas, params.m_p, xa, xb, color, clickedPosColor,
                         fillDepth);

  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  // TomDoingArt - update this for issue 1151?
  //if (fillGaps && closeGaps) {
  //  // Set the ink on gaps that were used to 4095
  //  TPixelCM32 *tempPix = refCMRaster->pixels(0);
  //  tempPix += (y * refCMRaster->getLx()) + xa - 1;
  //  int i = xa;
  //  while (i <= xb) {
  //    if (tempPix->getInk() == styleIndex) {
  //      tempPix->setInk(fakeStyleIndex);
  //    }
  //    tempPix++;
  //    i++;
  //  }
  //}

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    // continue if the fill runs over image bounding
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    // left end of the pixels to be filled
    pix = pix0 = ras->pixels(y) + xa;
    // right end of the pixels to be filled
    limit = ras->pixels(y) + xb;
    // left end of the fill seed pixels
    oldpix = ras->pixels(oldy) + xa;

    if (xsheet || fillGaps) {
      refpix    = refRas->pixels(y) + xa;
      oldrefpix = refRas->pixels(oldy) + xa;
    }

    x     = xa;
    oldxd = (std::numeric_limits<int>::min)();
    oldxc = (std::numeric_limits<int>::max)();

    // check pixels to right
    while (pix <= limit) {
      bool test = false;
      // check if the target is already in the range to be filled
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      bool canPaint = false;
      if (!xsheet && !fillGaps)
        canPaint = *pix != color && !test &&
                   floodCheck(clickedPosColor, pix, oldpix, fillDepth);
      else
        canPaint = *refpix != color && !test &&
                   floodCheck(clickedPosColor, refpix, oldrefpix, fillDepth);
      if (canPaint) {
        // compute horizontal range to be filled
        if (!xsheet && !fillGaps)
          fullColorFindSegment(ras, TPoint(x, y), xc, xd, color,
                               clickedPosColor, fillDepth);
        else
          fullColorFindSegment(refRas, TPoint(x, y), xc, xd, color,
                               clickedPosColor, fillDepth);
        // insert segment to be filled
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        
        // TomDoingArt - update this for issue 1151?
        //if (fillGaps && closeGaps) {
        //  // Set the ink on gaps that were used to 4095
        //  TPixelCM32 *tempPix = refCMRaster->pixels(0);
        //  tempPix += (y * refCMRaster->getLx()) + xa - 1;
        //  int i = xa;
        //  while (i <= xb) {
        //    if (tempPix->getInk() == styleIndex) {
        //      tempPix->setInk(fakeStyleIndex);
        //    }
        //    tempPix++;
        //    i++;
        //  }
        //}

        // create new fillSeed to invert direction, if needed
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        // jump to the next pixel to the right end of the range
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        if (xsheet || fillGaps) {
          refpix += xd - x + 1;
          oldrefpix += xd - x + 1;
        }
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
        if (xsheet || fillGaps) {
          refpix++;
          oldrefpix++;
        }
      }
    }
    // insert filled range as new fill seed
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  if (xsheet || fillGaps) refRas->unlock();

  // pixels are actually filled here
  TPixel32 premultiColor = premultiply(color);

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line                                 = ras->pixels(it->first);
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (saver) {
          saver->save(
              TRect(segment.first, it->first, segment.second, it->first));
        }
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (clickedPosColor.m == 0)
            *pix = pix->m == 0 ? color : overPix(color, *pix);
          else if (color.m == 0 || color.m == 255)  // used for erasing area
            *pix = color;
          else
            *pix = overPix(*pix, premultiColor);
        }
      }
    }
  }

  if (fillGaps && closeGaps) {
    TPixelCM32 *tempPix = refCMRaster->pixels();
    TPixel32 *keepPix   = ras->pixels();
    for (int tempY = 0; tempY < refCMRaster->getLy(); tempY++) {
      for (int tempX = 0; tempX < refCMRaster->getLx();
           tempX++, tempPix++, keepPix++) {
        if (tempPix->getInk() == fakeStyleIndex) {
          *keepPix = gapColor;
        }
      }
    }
  }
}
