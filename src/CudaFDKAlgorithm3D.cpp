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

#include "astra/CudaFDKAlgorithm3D.h"

#include <boost/lexical_cast.hpp>

#include "astra/AstraObjectManager.h"

#include "astra/ConeProjectionGeometry3D.h"

#include "../cuda/3d/astra3d.h"

using namespace std;

namespace astra {

// type of the algorithm, needed to register with CAlgorithmFactory
std::string CCudaFDKAlgorithm3D::type = "FDK_CUDA";

//----------------------------------------------------------------------------------------
// Constructor
CCudaFDKAlgorithm3D::CCudaFDKAlgorithm3D() 
{
	m_bIsInitialized = false;
	m_iGPUIndex = -1;
	m_iVoxelSuperSampling = 1;
}

//----------------------------------------------------------------------------------------
// Constructor with initialization
CCudaFDKAlgorithm3D::CCudaFDKAlgorithm3D(CProjector3D* _pProjector, 
								   CFloat32ProjectionData3DMemory* _pProjectionData, 
								   CFloat32VolumeData3DMemory* _pReconstruction)
{
	_clear();
	initialize(_pProjector, _pProjectionData, _pReconstruction);
}

//----------------------------------------------------------------------------------------
// Destructor
CCudaFDKAlgorithm3D::~CCudaFDKAlgorithm3D() 
{
	CReconstructionAlgorithm3D::_clear();
}


//---------------------------------------------------------------------------------------
// Check
bool CCudaFDKAlgorithm3D::_check()
{
	// check base class
	ASTRA_CONFIG_CHECK(CReconstructionAlgorithm3D::_check(), "CUDA_FDK", "Error in ReconstructionAlgorithm3D initialization");

	const CProjectionGeometry3D* projgeom = m_pSinogram->getGeometry();
	ASTRA_CONFIG_CHECK(dynamic_cast<const CConeProjectionGeometry3D*>(projgeom), "CUDA_FDK", "Error setting FDK geometry");

	return true;
}

//---------------------------------------------------------------------------------------
// Initialize - Config
bool CCudaFDKAlgorithm3D::initialize(const Config& _cfg)
{
	ASTRA_ASSERT(_cfg.self);
	ConfigStackCheck<CAlgorithm> CC("CudaFDKAlgorithm3D", this, _cfg);

	// if already initialized, clear first
	if (m_bIsInitialized) {
		clear();
	}

	// initialization of parent class
	if (!CReconstructionAlgorithm3D::initialize(_cfg)) {
		return false;
	}

	m_iGPUIndex = (int)_cfg.self->getOptionNumerical("GPUindex", -1);
	CC.markOptionParsed("GPUindex");
	m_iVoxelSuperSampling = (int)_cfg.self->getOptionNumerical("VoxelSuperSampling", 1);
	CC.markOptionParsed("VoxelSuperSampling");

	m_bShortScan = _cfg.self->getOptionBool("ShortScan", false);
	CC.markOptionParsed("ShortScan");

	// success
	m_bIsInitialized = _check();
	return m_bIsInitialized;
}

//----------------------------------------------------------------------------------------
// Initialize - C++
bool CCudaFDKAlgorithm3D::initialize(CProjector3D* _pProjector, 
								  CFloat32ProjectionData3DMemory* _pSinogram, 
								  CFloat32VolumeData3DMemory* _pReconstruction)
{
	// if already initialized, clear first
	if (m_bIsInitialized) {
		clear();
	}

	// required classes
	m_pProjector = _pProjector;
	m_pSinogram = _pSinogram;
	m_pReconstruction = _pReconstruction;

	// success
	m_bIsInitialized = _check();
	return m_bIsInitialized;
}

//---------------------------------------------------------------------------------------
// Information - All
map<string,boost::any> CCudaFDKAlgorithm3D::getInformation() 
{
	map<string, boost::any> res;
	return mergeMap<string,boost::any>(CAlgorithm::getInformation(), res);
};

//---------------------------------------------------------------------------------------
// Information - Specific
boost::any CCudaFDKAlgorithm3D::getInformation(std::string _sIdentifier) 
{
	return CAlgorithm::getInformation(_sIdentifier);
};

//----------------------------------------------------------------------------------------
// Iterate
void CCudaFDKAlgorithm3D::run(int _iNrIterations)
{
	// check initialized
	ASTRA_ASSERT(m_bIsInitialized);

	const CProjectionGeometry3D* projgeom = m_pSinogram->getGeometry();
	const CConeProjectionGeometry3D* conegeom = dynamic_cast<const CConeProjectionGeometry3D*>(projgeom);
	const CVolumeGeometry3D& volgeom = *m_pReconstruction->getGeometry();

	ASTRA_ASSERT(conegeom);

	CFloat32ProjectionData3DMemory* pSinoMem = dynamic_cast<CFloat32ProjectionData3DMemory*>(m_pSinogram);
	ASTRA_ASSERT(pSinoMem);
	CFloat32VolumeData3DMemory* pReconMem = dynamic_cast<CFloat32VolumeData3DMemory*>(m_pReconstruction);
	ASTRA_ASSERT(pReconMem);


	bool ok = true;

	ok = astraCudaFDK(pReconMem->getData(), pSinoMem->getDataConst(),
	                  volgeom.getGridColCount(),
	                  volgeom.getGridRowCount(),
	                  volgeom.getGridSliceCount(),
	                  conegeom->getProjectionCount(),
	                  conegeom->getDetectorColCount(),
	                  conegeom->getDetectorRowCount(),
	                  conegeom->getOriginSourceDistance(),
	                  conegeom->getOriginDetectorDistance(),
	                  conegeom->getDetectorSpacingX(),
	                  conegeom->getDetectorSpacingY(),
	                  conegeom->getProjectionAngles(),
	                  m_bShortScan, m_iGPUIndex, m_iVoxelSuperSampling);

	ASTRA_ASSERT(ok);

}
//----------------------------------------------------------------------------------------

} // namespace astra
