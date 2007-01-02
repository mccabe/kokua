/** 
 * @file llviewervisualparam.h
 * @brief viewer side visual params (with data file parsing)
 *
 * Copyright (c) 2001-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#ifndef LL_LLViewerVisualParam_H
#define LL_LLViewerVisualParam_H

#include "v3math.h"
#include "llstring.h"
#include "llvisualparam.h"

//-----------------------------------------------------------------------------
// LLViewerVisualParamInfo
//-----------------------------------------------------------------------------
class LLViewerVisualParamInfo : public LLVisualParamInfo
{
	friend class LLViewerVisualParam;
public:
	LLViewerVisualParamInfo();
	/*virtual*/ ~LLViewerVisualParamInfo();
	
	/*virtual*/ BOOL parseXml(LLXmlTreeNode* node);

protected:
	S32			mWearableType;
	LLString	mEditGroup;
	F32			mCamDist;
	F32			mCamAngle;		// degrees
	F32			mCamElevation;
	LLString	mCamTargetName;
	F32			mEditGroupDisplayOrder;
	BOOL		mShowSimple;	// show edit controls when in "simple ui" mode?
	F32			mSimpleMin;		// when in simple UI, apply this minimum, range 0.f to 100.f
	F32			mSimpleMax;		// when in simple UI, apply this maximum, range 0.f to 100.f
};

//-----------------------------------------------------------------------------
// LLViewerVisualParam
// VIRTUAL CLASS
// a viewer side interface class for a generalized parametric modification of the avatar mesh
//-----------------------------------------------------------------------------
class LLViewerVisualParam : public LLVisualParam
{
public:
	LLViewerVisualParam();
	/*virtual*/ ~LLViewerVisualParam(){};

	// Special: These functions are overridden by child classes
	LLViewerVisualParamInfo 	*getInfo() const { return (LLViewerVisualParamInfo*)mInfo; };
	//   This sets mInfo and calls initialization functions
	BOOL						setInfo(LLViewerVisualParamInfo *info);
	
	// LLVisualParam Virtual functions
	///*virtual*/ BOOL			parseData(LLXmlTreeNode* node);

	// New Virtual functions
	virtual F32					getTotalDistortion() = 0;
	virtual const LLVector3&	getAvgDistortion() = 0;
	virtual F32					getMaxDistortion() = 0;
	virtual LLVector3			getVertexDistortion(S32 index, LLPolyMesh *mesh) = 0;
	virtual const LLVector3*	getFirstDistortion(U32 *index, LLPolyMesh **mesh) = 0;
	virtual const LLVector3*	getNextDistortion(U32 *index, LLPolyMesh **mesh) = 0;
	
	// interface methods
	F32					getDisplayOrder()			{ return getInfo()->mEditGroupDisplayOrder; }
	S32					getWearableType() const		{ return getInfo()->mWearableType; }
	const LLString&		getEditGroup() const		{ return getInfo()->mEditGroup; }

	F32					getCameraDistance()	const	{ return getInfo()->mCamDist; } 
	F32					getCameraAngle() const		{ return getInfo()->mCamAngle; }  // degrees
	F32					getCameraElevation() const	{ return getInfo()->mCamElevation; } 
	const std::string&	getCameraTargetName() const { return getInfo()->mCamTargetName; }
	
	BOOL				getShowSimple() const		{ return getInfo()->mShowSimple; }
	F32					getSimpleMin() const		{ return getInfo()->mSimpleMin; }
	F32					getSimpleMax() const		{ return getInfo()->mSimpleMax; }
};

#endif // LL_LLViewerVisualParam_H
