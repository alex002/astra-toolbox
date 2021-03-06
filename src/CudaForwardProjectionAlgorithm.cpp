/*
-----------------------------------------------------------------------
Copyright: 2010-2015, iMinds-Vision Lab, University of Antwerp
           2014-2015, CWI, Amsterdam

Contact: astra@uantwerpen.be
Website: http://sf.net/projects/astra-toolbox

This file is part of the ASTRA Toolbox.


The ASTRA Toolbox is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

The ASTRA Toolbox is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the ASTRA Toolbox. If not, see <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------
$Id$
*/

#include "astra/CudaForwardProjectionAlgorithm.h"

#ifdef ASTRA_CUDA

#include "../cuda/2d/astra.h"

#include <driver_types.h>
#include <cuda_runtime_api.h>

#include <boost/lexical_cast.hpp>

#include "astra/AstraObjectManager.h"
#include "astra/FanFlatProjectionGeometry2D.h"
#include "astra/FanFlatVecProjectionGeometry2D.h"
#include "astra/CudaProjector2D.h"

#include "astra/Logging.h"

using namespace std;

namespace astra {

// type of the algorithm, needed to register with CAlgorithmFactory
std::string CCudaForwardProjectionAlgorithm::type = "FP_CUDA";

//----------------------------------------------------------------------------------------
// Constructor
CCudaForwardProjectionAlgorithm::CCudaForwardProjectionAlgorithm() 
{
	m_bIsInitialized = false;
}

//----------------------------------------------------------------------------------------
// Destructor
CCudaForwardProjectionAlgorithm::~CCudaForwardProjectionAlgorithm() 
{

}

//---------------------------------------------------------------------------------------
// Initialize - Config
bool CCudaForwardProjectionAlgorithm::initialize(const Config& _cfg)
{
	ASTRA_ASSERT(_cfg.self);
	ConfigStackCheck<CAlgorithm> CC("CudaForwardProjectionAlgorithm", this, _cfg);
	
	// sinogram data
	XMLNode* node = _cfg.self->getSingleNode("ProjectionDataId");
	ASTRA_CONFIG_CHECK(node, "FP_CUDA", "No ProjectionDataId tag specified.");
	int id = boost::lexical_cast<int>(node->getContent());
	m_pSinogram = dynamic_cast<CFloat32ProjectionData2D*>(CData2DManager::getSingleton().get(id));
	ASTRA_DELETE(node);
	CC.markNodeParsed("ProjectionDataId");

	// volume data
	node = _cfg.self->getSingleNode("VolumeDataId");
	ASTRA_CONFIG_CHECK(node, "FP_CUDA", "No VolumeDataId tag specified.");
	id = boost::lexical_cast<int>(node->getContent());
	m_pVolume = dynamic_cast<CFloat32VolumeData2D*>(CData2DManager::getSingleton().get(id));
	ASTRA_DELETE(node);
	CC.markNodeParsed("VolumeDataId");

	// GPU number
	m_iGPUIndex = (int)_cfg.self->getOptionNumerical("GPUindex", -1);
	m_iGPUIndex = (int)_cfg.self->getOptionNumerical("GPUIndex", m_iGPUIndex);
	CC.markOptionParsed("GPUindex");
	if (!_cfg.self->hasOption("GPUindex"))
		CC.markOptionParsed("GPUIndex");

	// Detector supersampling factor
	m_iDetectorSuperSampling = (int)_cfg.self->getOptionNumerical("DetectorSuperSampling", 1);
	CC.markOptionParsed("DetectorSuperSampling");


	// This isn't used yet, but passing it is not something to warn about
	node = _cfg.self->getSingleNode("ProjectorId");
	if (node) {
		id = boost::lexical_cast<int>(node->getContent());
		CProjector2D *projector = CProjector2DManager::getSingleton().get(id);
		if (!dynamic_cast<CCudaProjector2D*>(projector)) {
			ASTRA_WARN("non-CUDA Projector2D passed to FP_CUDA");
		}
		delete node;
	}
	CC.markNodeParsed("ProjectorId");
	


	// return success
	return check();
}

//----------------------------------------------------------------------------------------
// Initialize - C++
bool CCudaForwardProjectionAlgorithm::initialize(CProjectionGeometry2D* _pProjectionGeometry,
												 CVolumeGeometry2D* _pReconstructionGeometry,
												 CFloat32VolumeData2D* _pVolume,
												 CFloat32ProjectionData2D* _pSinogram,
												 int _iGPUindex, int _iDetectorSuperSampling)
{
	// store classes
	//m_pProjectionGeometry = _pProjectionGeometry;
	//m_pReconstructionGeometry = _pReconstructionGeometry;
	m_pVolume = _pVolume;
	m_pSinogram = _pSinogram;

	m_iDetectorSuperSampling = _iDetectorSuperSampling;
	m_iGPUIndex = _iGPUindex;

	// return success
	return check();
}

//----------------------------------------------------------------------------------------
// Check
bool CCudaForwardProjectionAlgorithm::check() 
{
	// check pointers
	ASTRA_CONFIG_CHECK(m_pSinogram, "FP_CUDA", "No valid projection data object found.");
	ASTRA_CONFIG_CHECK(m_pSinogram->isInitialized(), "FP_CUDA", "Projection data not initialized.");
	ASTRA_CONFIG_CHECK(m_pVolume, "FP_CUDA", "No valid volume data object found.");
	ASTRA_CONFIG_CHECK(m_pVolume->isInitialized(), "FP_CUDA", "Volume data not initialized.");

	// check restrictions
	//int iImageSideBlocks = m_pReconstructionGeometry->getGridColCount() / G_BLOCKIMAGESIZE;
	//ASTRA_CONFIG_CHECK((iImageSideBlocks * G_BLOCKIMAGESIZE) == m_pVolume->getWidth(), "FP_CUDA", "Volume Width must be a multiple of G_BLOCKIMAGESIZE");
	//ASTRA_CONFIG_CHECK((iImageSideBlocks * G_BLOCKIMAGESIZE) == m_pVolume->getHeight(), "FP_CUDA", "Volume Height must be a multiple of G_BLOCKIMAGESIZE");
	//ASTRA_CONFIG_CHECK(m_pProjectionGeometry->getDetectorCount() == (m_pVolume->getWidth() * 3 / 2), "SIRT_CUDA", "Number of detectors must be 1.5 times the width of the image");

	ASTRA_CONFIG_CHECK(m_iGPUIndex >= -1, "FP_CUDA", "GPUIndex must be a non-negative integer.");

	// success
	m_bIsInitialized = true;
	return true;
}

void CCudaForwardProjectionAlgorithm::setGPUIndex(int _iGPUIndex)
{
	m_iGPUIndex = _iGPUIndex;
}

//---------------------------------------------------------------------------------------
// Information - All
map<string,boost::any> CCudaForwardProjectionAlgorithm::getInformation() 
{
	map<string, boost::any> res;
	res["ProjectionGeometry"] = getInformation("ProjectionGeometry");
	res["ReconstructionGeometry"] = getInformation("ReconstructionGeometry");
	res["ProjectionDataId"] = getInformation("ProjectionDataId");
	res["VolumeDataId"] = getInformation("VolumeDataId");
	res["GPUindex"] = getInformation("GPUindex");
	res["DetectorSuperSampling"] = getInformation("DetectorSuperSampling");
	return mergeMap<string,boost::any>(CAlgorithm::getInformation(), res);
};

//---------------------------------------------------------------------------------------
// Information - Specific
boost::any CCudaForwardProjectionAlgorithm::getInformation(std::string _sIdentifier) 
{
	if (_sIdentifier == "ProjectionGeometry")	{ return string("not implemented"); }
	if (_sIdentifier == "ReconstructionGeometry")	{ return string("not implemented"); }
	if (_sIdentifier == "ProjectionDataId") {
		int iIndex = CData2DManager::getSingleton().getIndex(m_pSinogram);
		if (iIndex != 0) return iIndex;
		return std::string("not in manager");
	} 
	if (_sIdentifier == "VolumeDataId") {
		int iIndex = CData2DManager::getSingleton().getIndex(m_pVolume);
		if (iIndex != 0) return iIndex;
		return std::string("not in manager");
	}
	if (_sIdentifier == "GPUindex")	{ return m_iGPUIndex; }
	if (_sIdentifier == "DetectorSuperSampling")	{ return m_iDetectorSuperSampling; }
	return CAlgorithm::getInformation(_sIdentifier);
};

//----------------------------------------------------------------------------------------
// Run
void CCudaForwardProjectionAlgorithm::run(int)
{
	// check initialized
	assert(m_bIsInitialized);

	CVolumeGeometry2D* pVolGeom = m_pVolume->getGeometry();
	const CParallelProjectionGeometry2D* parProjGeom = dynamic_cast<CParallelProjectionGeometry2D*>(m_pSinogram->getGeometry());
	const CFanFlatProjectionGeometry2D* fanProjGeom = dynamic_cast<CFanFlatProjectionGeometry2D*>(m_pSinogram->getGeometry());
	const CFanFlatVecProjectionGeometry2D* fanVecProjGeom = dynamic_cast<CFanFlatVecProjectionGeometry2D*>(m_pSinogram->getGeometry());

	bool ok = false;
	if (parProjGeom) {

		float *offsets, *angles, detSize, outputScale;
		ok = convertAstraGeometry(pVolGeom, parProjGeom, offsets, angles, detSize, outputScale);

		ASTRA_ASSERT(ok); // FIXME

		// FIXME: Output scaling

		ok = astraCudaFP(m_pVolume->getDataConst(), m_pSinogram->getData(),
		                 pVolGeom->getGridColCount(), pVolGeom->getGridRowCount(),
		                 parProjGeom->getProjectionAngleCount(),
		                 parProjGeom->getDetectorCount(),
		                 angles, offsets, detSize,
		                 m_iDetectorSuperSampling, 1.0f * outputScale, m_iGPUIndex);

		delete[] offsets;
		delete[] angles;

	} else if (fanProjGeom || fanVecProjGeom) {

		astraCUDA::SFanProjection* projs;
		float outputScale;

		if (fanProjGeom) {
			ok = convertAstraGeometry(pVolGeom, fanProjGeom, projs, outputScale);
		} else {
			ok = convertAstraGeometry(pVolGeom, fanVecProjGeom, projs, outputScale);
		}

		ASTRA_ASSERT(ok);

		ok = astraCudaFanFP(m_pVolume->getDataConst(), m_pSinogram->getData(),
		                    pVolGeom->getGridColCount(), pVolGeom->getGridRowCount(),
		                    m_pSinogram->getGeometry()->getProjectionAngleCount(),
		                    m_pSinogram->getGeometry()->getDetectorCount(),
		                    projs,
		                    m_iDetectorSuperSampling, outputScale, m_iGPUIndex);

		delete[] projs;

	} else {

		ASTRA_ASSERT(false);

	}

	ASTRA_ASSERT(ok);

}

} // namespace astra

#endif // ASTRA_CUDA
