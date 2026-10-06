/**************************************************************************

  Copyright [2009] [CrypTool Team]

  This file is part of CrypTool.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

**************************************************************************/

//////////////////////////////////////////////////////////////////////
// PictureEx_wx.cpp: wxWidgets implementation of CPictureEx for platforms
// without OLE (IPicture). PictureEx.cpp remains the Windows implementation.
//
// GIF frames are decoded with wxGIFDecoder, other formats with wxImage.
// The memory DCs of the Windows version are replaced by wxImage canvases
// (m_pCanvas, m_pDispCanvas); the animation is advanced by a window timer
// on the GUI thread instead of a worker thread.
//
// PictureExLoop_wx.cpp compiles this file once more for CPictureExLoop.
//////////////////////////////////////////////////////////////////////

#ifndef PICTUREEX_WX_CLASS
#include "stdafx.h"
#include "PictureEx.h"
#define PICTUREEX_WX_CLASS CPictureEx
#endif

#include <wx/bitmap.h>
#include <wx/dc.h>
#include <wx/gifdecod.h>
#include <wx/image.h>
#include <wx/mstream.h>

namespace {

const UINT_PTR ANIMATION_TIMER_ID = 1;

void FillArea(wxImage &img, int x, int y, int w, int h, COLORREF clr)
{
	wxRect rect(x, y, w, h);
	rect.Intersect(wxRect(0, 0, img.GetWidth(), img.GetHeight()));
	if (!rect.IsEmpty())
		img.SetRGB(rect, GetRValue(clr), GetGValue(clr), GetBValue(clr));
}

// copies the rectangle (x, y, w, h) from src to dst (both of the same size)
void CopyArea(wxImage &dst, const wxImage &src, int x, int y, int w, int h)
{
	wxRect rect(x, y, w, h);
	rect.Intersect(wxRect(0, 0, dst.GetWidth(), dst.GetHeight()));
	rect.Intersect(wxRect(0, 0, src.GetWidth(), src.GetHeight()));
	if (rect.IsEmpty())
		return;
	for (int j = rect.y; j < rect.y + rect.height; j++)
		memcpy(dst.GetData() + 3 * (j * dst.GetWidth() + rect.x),
			src.GetData() + 3 * (j * src.GetWidth() + rect.x), 3 * rect.width);
}

// draws src onto dst at (x, y); masked or transparent pixels of src leave dst unchanged
void Compose(wxImage &dst, const wxImage &src, int x, int y)
{
	const int dw = dst.GetWidth(), dh = dst.GetHeight();
	const int sw = src.GetWidth(), sh = src.GetHeight();
	const unsigned char *s = src.GetData();
	const unsigned char *alpha = src.HasAlpha() ? src.GetAlpha() : NULL;
	const bool bMask = src.HasMask();
	const unsigned char mr = src.GetMaskRed(), mg = src.GetMaskGreen(), mb = src.GetMaskBlue();
	unsigned char *d = dst.GetData();

	for (int j = 0; j < sh; j++)
	{
		const int dy = y + j;
		if (dy < 0 || dy >= dh)
			continue;
		for (int i = 0; i < sw; i++)
		{
			const int dx = x + i;
			if (dx < 0 || dx >= dw)
				continue;
			const unsigned char *sp = s + 3 * (j * sw + i);
			unsigned char *dp = d + 3 * (dy * dw + dx);
			if (bMask && sp[0] == mr && sp[1] == mg && sp[2] == mb)
				continue;
			const unsigned a = alpha ? alpha[j * sw + i] : 255;
			for (int c = 0; c < 3; c++)
				dp[c] = (unsigned char)((sp[c] * a + dp[c] * (255 - a)) / 255);
		}
	}
}

UINT GifDisposal(wxAnimationDisposal disposal)
{
	switch (disposal)
	{
	case wxANIM_DONOTREMOVE:  return 1;
	case wxANIM_TOBACKGROUND: return 2;
	case wxANIM_TOPREVIOUS:   return 3;
	default:                  return 0;
	}
}

}

inline int PICTUREEX_WX_CLASS::TGIFLSDescriptor::GetPackedValue(enum LSDPackedValues Value)
{
	int nRet = (int)m_cPacked;

	switch (Value)
	{
	case LSD_PACKED_GLOBALCT:
		nRet = nRet >> 7;
		break;

	case LSD_PACKED_CRESOLUTION:
		nRet = ((nRet & 0x70) >> 4) + 1;
		break;

	case LSD_PACKED_SORT:
		nRet = (nRet & 8) >> 3;
		break;

	case LSD_PACKED_GLOBALCTSIZE:
		nRet &= 7;
		break;
	};

	return nRet;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PICTUREEX_WX_CLASS::PICTUREEX_WX_CLASS()
{
	m_pGIFLSDescriptor = NULL;
	m_pGIFHeader	   = NULL;
	m_pPicture		   = NULL;
	m_pCanvas		   = NULL;
	m_pDispCanvas	   = NULL;
	m_pRawData		   = NULL;
	m_hThread		   = NULL;
	m_hBitmap          = NULL;
	m_hOldBitmap       = NULL;
	m_hMemDC		   = NULL;
	m_hDispMemDC       = NULL;
	m_hDispMemBM       = NULL;
	m_hDispOldBM       = NULL;
	m_hExitEvent       = NULL;

	m_bIsInitialized   = FALSE;
	m_bExitThread	   = FALSE;
	m_bIsPlaying       = FALSE;
	m_bIsGIF		   = FALSE;
	m_clrBackground    = RGB(255,255,255); // white by default
	m_nGlobalCTSize    = 0;
	m_nCurrOffset	   = 0;
	m_nCurrFrame	   = 0;
	m_nDataSize		   = 0;
	m_PictureSize.cx = m_PictureSize.cy = 0;
	SetRect(&m_PaintRect,0,0,0,0);
}

PICTUREEX_WX_CLASS::~PICTUREEX_WX_CLASS()
{
	UnLoad();
}

BEGIN_MESSAGE_MAP(PICTUREEX_WX_CLASS, CStatic)
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL PICTUREEX_WX_CLASS::Load(HGLOBAL hGlobal, DWORD dwSize)
{
	UnLoad();

	if (!(m_pRawData = reinterpret_cast<unsigned char*> (GlobalLock(hGlobal))) )
	{
		TRACE(_T("Load: Error locking memory\n"));
		return FALSE;
	};

	m_nDataSize = dwSize;
	m_pGIFHeader = reinterpret_cast<TGIFHeader *> (m_pRawData);
	wxMemoryInputStream stream(m_pRawData, dwSize);

	if (dwSize < sizeof(TGIFHeader) + sizeof(TGIFLSDescriptor) ||
		memcmp(&m_pGIFHeader->m_cSignature,"GIF",3) != 0)
	{
		// it's not a GIF: let wxImage decode it
		if (!wxImage::FindHandler(wxBITMAP_TYPE_JPEG))
			wxInitAllImageHandlers();

		wxImage image;
		bool bLoaded = image.LoadFile(stream, wxBITMAP_TYPE_ANY);
		m_pRawData = NULL;
		m_pGIFHeader = NULL;
		GlobalUnlock(hGlobal);
		if (!bLoaded || !image.IsOk())
			return FALSE;

		m_pPicture = new wxImage(image);
		m_PictureSize.cx = image.GetWidth();
		m_PictureSize.cy = image.GetHeight();
	}
	else
	{
		// it's a GIF
		m_bIsGIF = TRUE;
		m_pGIFLSDescriptor = reinterpret_cast<TGIFLSDescriptor *>
			(m_pRawData + sizeof(TGIFHeader));
		if (m_pGIFLSDescriptor->GetPackedValue(LSD_PACKED_GLOBALCT) == 1)
		{
			// calculate the globat color table size
			m_nGlobalCTSize = static_cast<int>
				(3* (1 << (m_pGIFLSDescriptor->GetPackedValue(LSD_PACKED_GLOBALCTSIZE)+1)));
			// get the background color if GCT is present
			UINT nBkOffset = sizeof(TGIFHeader) + sizeof(TGIFLSDescriptor) + 3*m_pGIFLSDescriptor->m_cBkIndex;
			if (nBkOffset + 3 <= dwSize)
			{
				unsigned char *pBkClr = m_pRawData + nBkOffset;
				m_clrBackground = RGB(pBkClr[0],pBkClr[1],pBkClr[2]);
			}
		};

		// store the picture's size
		m_PictureSize.cx = m_pGIFLSDescriptor->m_wWidth;
		m_PictureSize.cy = m_pGIFLSDescriptor->m_wHeight;

		wxGIFDecoder decoder;
		wxGIFErrorCode err = decoder.LoadGIF(stream);
		UINT nFrameCount = (err == wxGIF_OK || err == wxGIF_TRUNCATED) ? decoder.GetFrameCount() : 0;

		m_pRawData = NULL;
		m_pGIFHeader = NULL;
		m_pGIFLSDescriptor = NULL;
		GlobalUnlock(hGlobal);

		if (nFrameCount == 0) // it's an empty GIF!
			return FALSE;

		for (UINT i = 0; i < nFrameCount; i++)
		{
			wxImage image;
			if (!decoder.ConvertToImage(i, &image) || !image.IsOk())
				continue;

			TFrame frame;
			wxSize size = decoder.GetFrameSize(i);
			wxPoint pos = decoder.GetFramePosition(i);
			long nDelay = decoder.GetDelay(i); // milliseconds
			frame.m_pPicture = new wxImage(image);
			frame.m_frameSize.cx = size.GetWidth();
			frame.m_frameSize.cy = size.GetHeight();
			frame.m_frameOffset.cx = pos.x;
			frame.m_frameOffset.cy = pos.y;
			frame.m_nDelay = nDelay > 0 ? (UINT)(nDelay / 10) : 0;
			frame.m_nDisposal = GifDisposal(decoder.GetDisposalMethod(i));
			m_arrFrames.push_back(frame);
		}

		if (m_arrFrames.empty()) // couldn't load any frames
			return FALSE;

		// a GIF with a single frame is not animated: Draw() renders m_arrFrames[0] like any other pic
	}; // if (!IsGIF...

	return PrepareDC(m_PictureSize.cx,m_PictureSize.cy);
}

void PICTUREEX_WX_CLASS::UnLoad()
{
	Stop();
	delete m_pPicture;
	m_pPicture = NULL;

	std::vector<TFrame>::iterator it;
	for (it=m_arrFrames.begin();it<m_arrFrames.end();it++)
		delete (*it).m_pPicture;
	m_arrFrames.clear();

	delete m_pCanvas;
	m_pCanvas = NULL;
	delete m_pDispCanvas;
	m_pDispCanvas = NULL;

	SetRect(&m_PaintRect,0,0,0,0);
	m_pGIFLSDescriptor = NULL;
	m_pGIFHeader	   = NULL;
	m_pRawData		   = NULL;
	m_hThread		   = NULL;
	m_bIsInitialized   = FALSE;
	m_bExitThread	   = FALSE;
	m_bIsGIF		   = FALSE;
	m_clrBackground    = RGB(255,255,255); // white by default
	m_nGlobalCTSize	   = 0;
	m_nCurrOffset	   = 0;
	m_nCurrFrame	   = 0;
	m_nDataSize		   = 0;
}

BOOL PICTUREEX_WX_CLASS::Draw()
{
	if (!m_bIsInitialized)
	{
		TRACE(_T("Call one of the CPictureEx::Load() member functions before calling Draw()\n"));
		return FALSE;
	};

	if (IsAnimatedGIF())
	{
		if (m_bIsPlaying)
			return TRUE;

		if (m_nCurrFrame >= m_arrFrames.size())
		{
			// a single run has finished: start again with the first frame
			m_nCurrFrame = 0;
			FillArea(*m_pCanvas, 0, 0, m_PictureSize.cx, m_PictureSize.cy, m_clrBackground);
		}

		// first, restore background (for stop/draw support)
		const TFrame &frame = m_arrFrames[m_nCurrFrame];
		if (frame.m_nDisposal == 2)
			FillArea(*m_pCanvas, frame.m_frameOffset.cx, frame.m_frameOffset.cy,
				frame.m_frameSize.cx, frame.m_frameSize.cy, m_clrBackground);
		else if (m_pDispCanvas && frame.m_nDisposal == 3)
		{
			CopyArea(*m_pCanvas, *m_pDispCanvas, frame.m_frameOffset.cx, frame.m_frameOffset.cy,
				frame.m_frameSize.cx, frame.m_frameSize.cy);
			delete m_pDispCanvas;
			m_pDispCanvas = NULL;
		}

		m_bIsPlaying = TRUE;
		ThreadAnimation();
		return TRUE;
	}

	if (m_arrFrames.size() == 1 && m_arrFrames[0].m_pPicture)
	{
		Compose(*m_pCanvas, *m_arrFrames[0].m_pPicture, m_arrFrames[0].m_frameOffset.cx, m_arrFrames[0].m_frameOffset.cy);
		if (m_hWnd)
			Invalidate(FALSE);
		return TRUE;
	};

	if (m_pPicture)
	{
		if (m_pPicture->GetWidth() == m_PictureSize.cx && m_pPicture->GetHeight() == m_PictureSize.cy)
			Compose(*m_pCanvas, *m_pPicture, 0, 0);
		else
			Compose(*m_pCanvas, m_pPicture->Scale(m_PictureSize.cx, m_PictureSize.cy, wxIMAGE_QUALITY_HIGH), 0, 0);
		if (m_hWnd)
			Invalidate(FALSE);
		return TRUE;
	};

	return FALSE;
}

// renders the current animation frame and schedules the next one
void PICTUREEX_WX_CLASS::ThreadAnimation()
{
	const TFrame &frame = m_arrFrames[m_nCurrFrame];
	UINT nDelay = 1;

	if (frame.m_pPicture)
	{
		// disposal method #3: store the background
		if (frame.m_nDisposal == 3)
		{
			delete m_pDispCanvas;
			m_pDispCanvas = new wxImage(m_pCanvas->Copy());
		};

		Compose(*m_pCanvas, *frame.m_pPicture, frame.m_frameOffset.cx, frame.m_frameOffset.cy);
		if (m_hWnd)
			Invalidate(FALSE);

		// if the delay time is too short (like in old GIFs), wait for 100ms
		nDelay = frame.m_nDelay < 5 ? 100 : 10*frame.m_nDelay;
	};

	if (m_hWnd)
		SetTimer(ANIMATION_TIMER_ID, nDelay, NULL);
	else
		m_bIsPlaying = FALSE;
}

void PICTUREEX_WX_CLASS::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != ANIMATION_TIMER_ID)
	{
		CStatic::OnTimer(nIDEvent);
		return;
	}

	KillTimer(ANIMATION_TIMER_ID);
	if (!m_bIsPlaying || m_nCurrFrame >= m_arrFrames.size())
		return;

	const TFrame &frame = m_arrFrames[m_nCurrFrame];
	if (frame.m_nDisposal == 2)
		FillArea(*m_pCanvas, frame.m_frameOffset.cx, frame.m_frameOffset.cy,
			frame.m_frameSize.cx, frame.m_frameSize.cy, m_clrBackground);
	else if (m_pDispCanvas && frame.m_nDisposal == 3)
	{
		CopyArea(*m_pCanvas, *m_pDispCanvas, frame.m_frameOffset.cx, frame.m_frameOffset.cy,
			frame.m_frameSize.cx, frame.m_frameSize.cy);
		delete m_pDispCanvas;
		m_pDispCanvas = NULL;
	}

	m_nCurrFrame++;
	if (m_nCurrFrame == m_arrFrames.size())
	{
// Animation nur ein einziges Mal abspielen?
#ifdef PLAY_ANIM_ONLY_ONCE
		m_bIsPlaying = FALSE;
		return;
#else
		m_nCurrFrame = 0;
		// init the screen for the first frame,
		FillArea(*m_pCanvas, 0, 0, m_PictureSize.cx, m_PictureSize.cy, m_clrBackground);
#endif
	};

	ThreadAnimation();
}

SIZE PICTUREEX_WX_CLASS::GetSize() const
{
	return m_PictureSize;
}

BOOL PICTUREEX_WX_CLASS::Load(LPCTSTR szFileName)
{
	ASSERT(szFileName);

	CFile file;
	if (!file.Open(szFileName, CFile::modeRead | CFile::shareDenyWrite))
	{
		TRACE(_T("Load (file): Error opening file %s\n"),szFileName);
		return FALSE;
	};

	DWORD dwSize = (DWORD)file.GetLength();
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE,dwSize);
	if (!hGlobal)
	{
		TRACE(_T("Load (file): Error allocating memory\n"));
		return FALSE;
	};

	char *pData = reinterpret_cast<char*>(GlobalLock(hGlobal));
	if (!pData)
	{
		TRACE(_T("Load (file): Error locking memory\n"));
		GlobalFree(hGlobal);
		return FALSE;
	};

	UINT nRead = 0;
	TRY
	{
		nRead = file.Read(pData,dwSize);
	}
	CATCH(CFileException, e)
	{
		TRACE(_T("Load (file): An exception occured while reading the file %s\n"), szFileName);
		e->Delete();
	}
	END_CATCH
	GlobalUnlock(hGlobal);
	file.Close();

	BOOL bRetValue = nRead == dwSize && Load(hGlobal,dwSize);
	GlobalFree(hGlobal);
	return bRetValue;
}

BOOL PICTUREEX_WX_CLASS::Load(LPCTSTR szResourceName, LPCTSTR szResourceType)
{
	ASSERT(szResourceName);
	ASSERT(szResourceType);

	HRSRC hPicture = FindResource(AfxGetResourceHandle(),szResourceName,szResourceType);
	HGLOBAL hResData;
	if (!hPicture || !(hResData = LoadResource(AfxGetResourceHandle(),hPicture)))
	{
		TRACE(_T("Load (resource): Error loading resource %s\n"),szResourceName);
		return FALSE;
	};
	DWORD dwSize = SizeofResource(AfxGetResourceHandle(),hPicture);

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE,dwSize);
	if (!hGlobal)
	{
		TRACE(_T("Load (resource): Error allocating memory\n"));
		FreeResource(hResData);
		return FALSE;
	};

	char *pDest = reinterpret_cast<char *> (GlobalLock(hGlobal));
	char *pSrc = reinterpret_cast<char *> (LockResource(hResData));
	if (!pSrc || !pDest)
	{
		TRACE(_T("Load (resource): Error locking memory\n"));
		GlobalFree(hGlobal);
		FreeResource(hResData);
		return FALSE;
	};
	CopyMemory(pDest,pSrc,dwSize);
	FreeResource(hResData);
	GlobalUnlock(hGlobal);

	BOOL bRetValue = Load(hGlobal,dwSize);
	GlobalFree(hGlobal);
	return bRetValue;
}

void PICTUREEX_WX_CLASS::Stop()
{
	m_bIsPlaying = FALSE;
	if (m_hWnd && ::IsWindow(m_hWnd))
		KillTimer(ANIMATION_TIMER_ID);
	m_bExitThread = FALSE;
}

BOOL PICTUREEX_WX_CLASS::IsGIF() const
{
	return m_bIsGIF;
}

BOOL PICTUREEX_WX_CLASS::IsAnimatedGIF() const
{
	return (m_bIsGIF && (m_arrFrames.size() > 1));
}

BOOL PICTUREEX_WX_CLASS::IsPlaying() const
{
	return m_bIsPlaying;
}

int PICTUREEX_WX_CLASS::GetFrameCount() const
{
	if (!IsAnimatedGIF())
		return 0;

	return (int)m_arrFrames.size();
}

COLORREF PICTUREEX_WX_CLASS::GetBkColor() const
{
	return m_clrBackground;
}

void PICTUREEX_WX_CLASS::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	wxDC *pDC = dc.GetWx();
	if (!m_pCanvas || !pDC)
		return;

	LONG nPaintWidth = m_PaintRect.right-m_PaintRect.left;

	if (nPaintWidth > 0)
	{
		wxRect rect(m_PaintRect.left, m_PaintRect.top, nPaintWidth, m_PaintRect.bottom - m_PaintRect.top);
		rect.Intersect(wxRect(0, 0, m_pCanvas->GetWidth(), m_pCanvas->GetHeight()));
		if (!rect.IsEmpty())
			pDC->DrawBitmap(wxBitmap(m_pCanvas->GetSubImage(rect)),
				rect.x - m_PaintRect.left, rect.y - m_PaintRect.top, false);
	}
	else
	{
		pDC->DrawBitmap(wxBitmap(*m_pCanvas), 0, 0, false);
	};
}

BOOL PICTUREEX_WX_CLASS::PrepareDC(int nWidth, int nHeight)
{
	if (m_hWnd)
		SetWindowPos(NULL,0,0,nWidth,nHeight,SWP_NOMOVE | SWP_NOZORDER);

	if (nWidth <= 0 || nHeight <= 0)
		return FALSE;

	m_pCanvas = new wxImage(nWidth, nHeight, false);

	// fill the background
	m_clrBackground = GetSysColor(COLOR_3DFACE);
	FillArea(*m_pCanvas, 0, 0, nWidth, nHeight, m_clrBackground);

	m_bIsInitialized = TRUE;
	return TRUE;
}

void PICTUREEX_WX_CLASS::OnDestroy()
{
	Stop();
	CStatic::OnDestroy();
}

void PICTUREEX_WX_CLASS::SetBkColor(COLORREF clr)
{
	if (!m_bIsInitialized) return;

	m_clrBackground = clr;
	FillArea(*m_pCanvas, 0, 0, m_PictureSize.cx, m_PictureSize.cy, clr);
}

BOOL PICTUREEX_WX_CLASS::SetPaintRect(const RECT *lpRect)
{
	return CopyRect(&m_PaintRect, lpRect);
}

BOOL PICTUREEX_WX_CLASS::GetPaintRect(RECT *lpRect)
{
	return CopyRect(lpRect, &m_PaintRect);
}
