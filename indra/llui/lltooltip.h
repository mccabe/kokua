/** 
 * @file lltooltip.h
 * @brief LLToolTipMgr class definition and related classes
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLTOOLTIP_H
#define LL_LLTOOLTIP_H

// Library includes
#include "llsingleton.h"
#include "llinitparam.h"
#include "llview.h"

//
// Classes
//
class LLToolTipView : public LLView
{
public:
	struct Params : public LLInitParam::Block<Params, LLView::Params>
	{
		Params()
		{
			mouse_opaque = false;
		}
	};
	LLToolTipView(const LLToolTipView::Params&);
	/*virtual*/ BOOL handleHover(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleMiddleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleScrollWheel( S32 x, S32 y, S32 clicks );

	/*virtual*/ void onMouseLeave(S32 x, S32 y, MASK mask);

	void drawStickyRect();

	/*virtual*/ void draw();
};

struct LLToolTipPosParams : public LLInitParam::Block<LLToolTipPosParams>
{
	Mandatory<S32>		x, 
						y;
	LLToolTipPosParams()
	:	x("x"),
		y("y")
	{}
};

struct LLToolTipParams : public LLInitParam::Block<LLToolTipParams>
{
	typedef boost::function<void(void)> click_callback_t;

	Mandatory<std::string>		message;
	
	Optional<LLToolTipPosParams>	pos;
	Optional<F32>					delay_time,
									visible_time;
	Optional<LLRect>				sticky_rect;
	Optional<S32>					width;
	Optional<LLUIImage*>			image;

	Optional<click_callback_t>		click_callback;

	LLToolTipParams();
	LLToolTipParams(const std::string& message);
};

class LLToolTipMgr : public LLSingleton<LLToolTipMgr>
{
	LOG_CLASS(LLToolTipMgr);
public:
	LLToolTipMgr();
	void show(const LLToolTipParams& params);
	void show(const std::string& message);

	void enableToolTips();
	void hideToolTips();
	bool toolTipVisible();
	LLRect getToolTipRect();

	LLRect getStickyRect();

private:
	class LLToolTip* createToolTip(const LLToolTipParams& params);

	bool				mToolTipsBlocked;
	class LLToolTip*	mToolTip;
	std::string			mLastToolTipMessage;
	LLRect				mToolTipStickyRect;
};

//
// Globals
//

extern LLToolTipView *gToolTipView;

#endif