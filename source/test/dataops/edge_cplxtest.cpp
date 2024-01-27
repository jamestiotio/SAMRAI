/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2024 Lawrence Livermore National Security, LLC
 * Description:   Main program to test edge-centered complex patch data ops
 *
 ************************************************************************/

#include "SAMRAI/SAMRAI_config.h"

#include "SAMRAI/tbox/SAMRAI_MPI.h"
#include "SAMRAI/tbox/PIO.h"

#include "SAMRAI/tbox/SAMRAIManager.h"

#include "SAMRAI/hier/Box.h"
#include "SAMRAI/hier/BoxContainer.h"
#include "SAMRAI/geom/CartesianGridGeometry.h"
#include "SAMRAI/geom/CartesianPatchGeometry.h"
#include "SAMRAI/pdat/EdgeData.h"
#include "SAMRAI/math/HierarchyDataOpsComplex.h"
#include "SAMRAI/math/HierarchyEdgeDataOpsComplex.h"
#include "SAMRAI/math/HierarchyDataOpsReal.h"
#include "SAMRAI/math/HierarchyEdgeDataOpsReal.h"
#include "SAMRAI/pdat/EdgeIndex.h"
#include "SAMRAI/pdat/EdgeIterator.h"
#include "SAMRAI/pdat/EdgeVariable.h"
#include "SAMRAI/hier/Index.h"
#include "SAMRAI/hier/IntVector.h"
#include "SAMRAI/hier/Patch.h"
#include "SAMRAI/hier/PatchDescriptor.h"
#include "SAMRAI/hier/PatchHierarchy.h"
#include "SAMRAI/hier/PatchLevel.h"
#include "SAMRAI/tbox/Complex.h"
#include "SAMRAI/tbox/Utilities.h"
#include "SAMRAI/tbox/MathUtilities.h"
#include "SAMRAI/hier/VariableDatabase.h"
#include "SAMRAI/hier/VariableContext.h"

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iomanip>
#include <memory>

using namespace SAMRAI;

/* Helper function declarations */
static bool
complexDataSameAsValue(
   int desc_id,
   dcomplex value,
   std::shared_ptr<hier::PatchHierarchy> hierarchy);
static bool
doubleDataSameAsValue(
   int desc_id,
   double value,
   std::shared_ptr<hier::PatchHierarchy> hierarchy);

#define NVARS 4

int main(
   int argc,
   char* argv[]) {

   int num_failures = 0;

   tbox::SAMRAI_MPI::init(&argc, &argv);
   tbox::SAMRAIManager::initialize();
   tbox::SAMRAIManager::startup();

   if (argc < 2) {
      TBOX_ERROR("Usage: " << argv[0] << " [dimension]");
   }

   const unsigned short d = static_cast<unsigned short>(atoi(argv[1]));
   TBOX_ASSERT(d > 0);
   TBOX_ASSERT(d <= SAMRAI::MAX_DIM_VAL);
   const tbox::Dimension dim(d);

   const std::string log_fn = std::string("edge_cplxtest.")
      + tbox::Utilities::intToString(dim.getValue(), 1) + "d.log";
   tbox::PIO::logAllNodes(log_fn);

   /*
    * Create block to force pointer deallocation.  If this is not done
    * then there will be memory leaks reported.
    */
   {
      int ln, iv;

      // Make a dummy hierarchy domain
      double lo[SAMRAI::MAX_DIM_VAL];
      double hi[SAMRAI::MAX_DIM_VAL];

      hier::Index clo0(dim);
      hier::Index chi0(dim);
      hier::Index clo1(dim);
      hier::Index chi1(dim);
      hier::Index flo0(dim);
      hier::Index fhi0(dim);
      hier::Index flo1(dim);
      hier::Index fhi1(dim);

      for (int i = 0; i < dim.getValue(); ++i) {
         lo[i] = 0.0;
         clo0(i) = 0;
         flo0(i) = 4;
         fhi0(i) = 7;
         if (i == 1) {
            hi[i] = 0.5;
            chi0(i) = 2;
            clo1(i) = 3;
            chi1(i) = 4;
         } else {
            hi[i] = 1.0;
            chi0(i) = 9;
            clo1(i) = 0;
            chi1(i) = 9;
         }
         if (i == 0) {
            flo1(i) = 8;
            fhi1(i) = 13;
         } else {
            flo1(i) = flo0(i);
            fhi1(i) = fhi0(i);
         }
      }

      hier::Box coarse0(clo0, chi0, hier::BlockId(0));
      hier::Box coarse1(clo1, chi1, hier::BlockId(0));
      hier::Box fine0(flo0, fhi0, hier::BlockId(0));
      hier::Box fine1(flo1, fhi1, hier::BlockId(0));
      hier::IntVector ratio(dim, 2);

      hier::BoxContainer coarse_domain;
      hier::BoxContainer fine_boxes;
      coarse_domain.pushBack(coarse0);
      coarse_domain.pushBack(coarse1);
      fine_boxes.pushBack(fine0);
      fine_boxes.pushBack(fine1);

      std::shared_ptr<geom::CartesianGridGeometry> geometry(
         new geom::CartesianGridGeometry(
            "CartesianGeometry",
            lo,
            hi,
            coarse_domain));

      std::shared_ptr<hier::PatchHierarchy> hierarchy(
         new hier::PatchHierarchy("PatchHierarchy", geometry));

      hierarchy->setMaxNumberOfLevels(2);
      hierarchy->setRatioToCoarserLevel(ratio, 1);

      const tbox::SAMRAI_MPI& mpi(tbox::SAMRAI_MPI::getSAMRAIWorld());
      const int nproc = mpi.getSize();

      const int n_coarse_boxes = coarse_domain.size();
      const int n_fine_boxes = fine_boxes.size();

      std::shared_ptr<hier::BoxLevel> layer0(
         std::make_shared<hier::BoxLevel>(
            hier::IntVector(dim, 1), geometry));
      std::shared_ptr<hier::BoxLevel> layer1(
         std::make_shared<hier::BoxLevel>(ratio, geometry));

      hier::BoxContainer::iterator coarse_itr = coarse_domain.begin();
      for (int ib = 0; ib < n_coarse_boxes; ++ib, ++coarse_itr) {
         if (nproc > 1) {
            if (ib == layer0->getMPI().getRank()) {
               layer0->addBox(hier::Box(*coarse_itr, hier::LocalId(ib),
                     layer0->getMPI().getRank()));
            }
         } else {
            layer0->addBox(hier::Box(*coarse_itr, hier::LocalId(ib), 0));
         }
      }

      hier::BoxContainer::iterator fine_itr = fine_boxes.begin();
      for (int ib = 0; ib < n_fine_boxes; ++ib, ++fine_itr) {
         if (nproc > 1) {
            if (ib == layer1->getMPI().getRank()) {
               layer1->addBox(hier::Box(*fine_itr, hier::LocalId(ib),
                     layer1->getMPI().getRank()));
            }
         } else {
            layer1->addBox(hier::Box(*fine_itr, hier::LocalId(ib), 0));
         }
      }

      hierarchy->makeNewPatchLevel(0, layer0);
      hierarchy->makeNewPatchLevel(1, layer1);

      // Create instance of hier::Variable database
      hier::VariableDatabase* variable_db = hier::VariableDatabase::getDatabase();
      std::shared_ptr<hier::VariableContext> dummy(
         variable_db->getContext("dummy"));
      const hier::IntVector no_ghosts(dim, 0);

      // Make some dummy variables and data on the hierarchy
      std::shared_ptr<pdat::EdgeVariable<dcomplex> > fvar[NVARS];
      int svindx[NVARS];
      fvar[0].reset(new pdat::EdgeVariable<dcomplex>(dim, "fvar0", 1));
      svindx[0] = variable_db->registerVariableAndContext(
            fvar[0], dummy, no_ghosts);
      fvar[1].reset(new pdat::EdgeVariable<dcomplex>(dim, "fvar1", 1));
      svindx[1] = variable_db->registerVariableAndContext(
            fvar[1], dummy, no_ghosts);
      fvar[2].reset(new pdat::EdgeVariable<dcomplex>(dim, "fvar2", 1));
      svindx[2] = variable_db->registerVariableAndContext(
            fvar[2], dummy, no_ghosts);
      fvar[3].reset(new pdat::EdgeVariable<dcomplex>(dim, "fvar3", 1));
      svindx[3] = variable_db->registerVariableAndContext(
            fvar[3], dummy, no_ghosts);

      std::shared_ptr<pdat::EdgeVariable<double> > swgt(
         new pdat::EdgeVariable<double>(dim, "swgt", 1));
      int swgt_id = variable_db->registerVariableAndContext(
            swgt, dummy, no_ghosts);

      // allocate data on hierarchy
      for (ln = 0; ln < 2; ++ln) {
         hierarchy->getPatchLevel(ln)->allocatePatchData(swgt_id);
         for (iv = 0; iv < NVARS; ++iv) {
            hierarchy->getPatchLevel(ln)->allocatePatchData(svindx[iv]);
         }
      }

      std::shared_ptr<math::HierarchyDataOpsComplex> edge_ops(
         new math::HierarchyEdgeDataOpsComplex(
            hierarchy,
            0,
            1));
      TBOX_ASSERT(edge_ops);

      std::shared_ptr<math::HierarchyDataOpsReal<double> > swgt_ops(
         new math::HierarchyEdgeDataOpsReal<double>(
            hierarchy,
            0,
            1));

      std::shared_ptr<hier::Patch> patch;

      // Initialize control volume data for edge-centered components
      hier::Box coarse_fine = fine0 + fine1;
      coarse_fine.coarsen(ratio);
      for (ln = 0; ln < 2; ++ln) {
         std::shared_ptr<hier::PatchLevel> level(
            hierarchy->getPatchLevel(ln));
         for (hier::PatchLevel::iterator ip(level->begin());
              ip != level->end(); ++ip) {
            patch = *ip;
            std::shared_ptr<geom::CartesianPatchGeometry> pgeom(
               SAMRAI_SHARED_PTR_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
                  patch->getPatchGeometry()));
            TBOX_ASSERT(pgeom);
            const double* dx = pgeom->getDx();
            double edge_vol = dx[0];
            for (int i = 1; i < dim.getValue(); ++i) {
               edge_vol *= dx[i];
            }
            std::shared_ptr<pdat::EdgeData<double> > data(
               SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<double>, hier::PatchData>(
                  patch->getPatchData(swgt_id)));
            TBOX_ASSERT(data);
            data->fillAll(edge_vol);
#if defined(HAVE_RAJA)
            tbox::parallel_synchronize();
#endif
            pdat::EdgeIndex fi(dim);

            if (dim.getValue() == 2) {
               int plo0 = patch->getBox().lower(0);
               int phi0 = patch->getBox().upper(0);
               int plo1 = patch->getBox().lower(1);
               int phi1 = patch->getBox().upper(1);
               int ic;

               if (ln == 0) {
                  data->fillAll(0.0, (coarse_fine * patch->getBox()));
#if defined(HAVE_RAJA)
                  tbox::parallel_synchronize();
#endif

                  if (patch->getLocalId() == 0) {
                     // bottom edge boundaries
                     for (ic = plo0; ic <= phi0; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(ic, plo1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 0.5;
                     }
                     // left and right edge boundaries
                     for (ic = plo1; ic <= phi1; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 0.5;
                     }
                  } else {
                     // top and bottom edge boundaries
                     for (ic = plo0; ic <= phi0; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(ic, plo1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(ic, phi1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 0.5;
                     }
                     // left and right edge boundaries
                     for (ic = plo1; ic <= phi1; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 0.5;
                     }
                  }
               } else {
                  if (patch->getLocalId() == 0) {
                     // top and bottom coarse-fine edge boundaries
                     for (ic = plo0; ic <= phi0; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(ic, plo1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic, phi1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 1.5;
                     }
                     // left coarse-fine edge boundaries
                     for (ic = plo1; ic <= phi1; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 1.5;
                     }
                  } else {
                     // top and bottom coarse-fine edge boundaries
                     for (ic = plo0; ic <= phi0; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(ic, plo1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic, phi1),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 1.5;
                     }
                     // left and right coarse-fine edge boundaries
                     for (ic = plo1; ic <= phi1; ++ic) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Lower);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::Upper);
                        (*data)(fi) *= 1.5;
                     }
                  }
               }
            } else {
               int plo0 = patch->getBox().lower(0);
               int phi0 = patch->getBox().upper(0);
               int plo1 = patch->getBox().lower(1);
               int phi1 = patch->getBox().upper(1);
               int plo2 = patch->getBox().lower(2);
               int phi2 = patch->getBox().upper(2);
               int ic0, ic1, ic2;

               if (ln == 0) {
                  data->fillAll(0.0, (coarse_fine * patch->getBox()));
#if defined(HAVE_RAJA)
                  tbox::parallel_synchronize();
#endif

                  if (patch->getLocalId() == 0) {
                     // front and back face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 0.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                     }
                     // bottom face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic2 = plo2; ic2 < phi2; ++ic2) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 0.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                     }
                     // left and right face boundary edges
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                        }
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     // front and back top and bottom edges
                     for (ic0 = plo0; ic0 <= phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                     }
                     // left and right top and bottom edges
                     for (ic2 = plo2; ic2 <= phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     // left and right front and back edges
                     for (ic1 = plo1; ic1 <= phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.25;
                     }
                  } else {
                     // front and back face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 0.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     // top and bottom face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic2 = plo2; ic2 < phi2; ++ic2) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) = 0.0;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) = 0.0;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) = 0.0;
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) = 0.0;
                     }
                     // left and right face boundary edges
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 0.5;
                        }
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.5;
                     }
                     // front and back top and bottom edges
                     for (ic0 = plo0; ic0 <= phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) = 0.0;
                     }
                     // left and right top and bottom edges
                     for (ic2 = plo2; ic2 <= phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) = 0.0;
                     }
                     // left and right front and back edges
                     for (ic1 = plo1; ic1 <= phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 0.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 0.25;
                     }
                  }
               } else {
                  if (patch->getLocalId() == 0) {
                     // front and back face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     // top and bottom face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic2 = plo2; ic2 < phi2; ++ic2) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                     // left face boundary edges
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                     // front and back top and bottom edges
                     for (ic0 = plo0; ic0 <= phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 2.25;
                     }
                     // left and right top and bottom edges
                     for (ic2 = plo2; ic2 <= phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     // left and right front and back edges
                     for (ic1 = plo1; ic1 <= phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                  } else {
                     // front and back face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, phi2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, ic1, plo2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     // top and bottom face boundary edges
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        for (ic2 = plo2; ic2 < phi2; ++ic2) {
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, phi1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::X,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(ic0, plo1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic0 = plo0; ic0 < phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 1.5;
                     }
                     // left and right face boundary edges
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        for (ic1 = plo1; ic1 < phi1; ++ic1) {
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperLeft);
                           (*data)(fi) = 0.0;
                           fi = pdat::EdgeIndex(hier::Index(plo0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::LowerRight);
                           (*data)(fi) = 0.0;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Z,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                           fi = pdat::EdgeIndex(hier::Index(phi0, ic1, ic2),
                                 pdat::EdgeIndex::Y,
                                 pdat::EdgeIndex::UpperRight);
                           (*data)(fi) *= 1.5;
                        }
                     }
                     for (ic2 = plo2; ic2 < phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                     }
                     for (ic1 = plo1; ic1 < phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 1.5;
                     }
                     // front and back top and bottom edges
                     for (ic0 = plo0; ic0 <= phi0; ++ic0) {
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, phi1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, phi2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(ic0, plo1, plo2),
                              pdat::EdgeIndex::X,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) *= 2.25;
                     }
                     // left and right top and bottom edges
                     for (ic2 = plo2; ic2 <= phi2; ++ic2) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, phi1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(plo0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, plo1, ic2),
                              pdat::EdgeIndex::Z,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) *= 2.25;
                     }
                     // left and right front and back edges
                     for (ic1 = plo1; ic1 <= phi1; ++ic1) {
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerRight);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(plo0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::LowerLeft);
                        (*data)(fi) = 0.0;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, phi2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperRight);
                        (*data)(fi) *= 2.25;
                        fi = pdat::EdgeIndex(hier::Index(phi0, ic1, plo2),
                              pdat::EdgeIndex::Y,
                              pdat::EdgeIndex::UpperLeft);
                        (*data)(fi) *= 2.25;
                     }
                  }
               }
            }
         }
      }

      // Test #1: Print out control volume data and compute its integral

      // Test #1a: Check control volume data set properly
      // Expected: cwgt = 0.01 on coarse (except where finer patch exists) and
      // 0.0025 on fine level
/*   bool vol_test_passed = true;
 *   for (ln = 0; ln < 2; ++ln) {
 *   for (hier::PatchLevel::iterator ip(hierarchy->getPatchLevel(ln)->begin()); ip != hierarchy->getPatchLevel(ln)->end(); ++ip) {
 *   patch = hierarchy->getPatchLevel(ln)->getPatch(ip());
 *   std::shared_ptr< pdat::CellData<double> > svdata = patch->getPatchData(cwgt_id);
 *
 *   pdat::CellIterator cend(pdat::CellGeometry::end(svdata->getBox()));
 *   for (pdat::CellIterator c(pdat::CellGeometry::begin(svdata->getBox())); c != cend && vol_test_passed; ++c) {
 *   pdat::CellIndex cell_index = *c;
 *
 *   if (ln == 0) {
 *   if ((coarse_fine * patch->getBox()).contains(cell_index)) {
 *   if ( !tbox::MathUtilities<double>::equalEps((*svdata)(cell_index),0.0) ) {
 *   vol_test_passed = false;
 *   }
 *   } else {
 *   if ( !tbox::MathUtilities<double>::equalEps((*svdata)(cell_index),0.01) ) {
 *   vol_test_passed = false;
 *   }
 *   }
 *   }
 *
 *   if (ln == 1) {
 *   if ( !tbox::MathUtilities<double>::equalEps((*svdata)(cell_index),0.0025) ) {
 *   vol_test_passed = false;
 *   }
 *   }
 *   }
 *   }
 *   }
 *   if (!vol_test_passed) {
 *   ++num_failures;
 *   tbox::perr << "FAILED: - Test #1a: Check control volume data set properly" << std::endl;
 *   cwgt_ops->printData(cwgt_id, tbox::plog);
 *   }
 */
/*   tbox::plog << "edge control volume data" << std::endl;
 *   swgt_ops->printData(swgt_id, tbox::plog);
 */

      // Test #1b: math::HierarchyEdgeDataOpsComplex::sumControlVolumes()
      // Expected: norm = 1.0
      double norm = edge_ops->sumControlVolumes(svindx[0], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      {
         double compare;
         if (dim.getValue() == 2) {
            compare = 1.0;
         } else {
            compare = 1.5;
         }
         if (!tbox::MathUtilities<double>::equalEps(norm, compare)) {
            ++num_failures;
            tbox::perr
            << "FAILED: - Test #1b: math::HierarchyEdgeDataOpsComplex::sumControlVolumes()\n"
            << "Expected value = " << compare << ", Computed value = "
            << norm << std::endl;
         }
      }

      // Test #2: math::HierarchyEdgeDataOpsComplex::numberOfEntries()
      // Expected: num_data_points = 209
      size_t num_data_points = edge_ops->numberOfEntries(svindx[0]);
      {
         size_t compare;
         if (dim.getValue() == 2) {
            compare = 209;
         } else {
            compare = 2615;
         }
         if (num_data_points != compare) {
            ++num_failures;
            tbox::perr
            << "FAILED: - Test #2: math::HierarchyEdgeDataOpsComplex::numberOfEntries()\n"
            << "Expected value = " << compare << ", Computed value = "
            << num_data_points << std::endl;
         }
      }

      // Test #3a: math::HierarchyEdgeDataOpsComplex::setToScalar()
      // Expected: v0 = (2.0,1.5)
      dcomplex val0 = dcomplex(2.0, 1.5);
      edge_ops->setToScalar(svindx[0], val0);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!complexDataSameAsValue(svindx[0], val0, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #3a: math::HierarchyEdgeDataOpsComplex::setToScalar()\n"
         << "Expected: v0 = " << val0 << std::endl;
         edge_ops->printData(svindx[0], tbox::plog);
      }

      // Test #3b: math::HierarchyEdgeDataOpsComplex::setToScalar()
      // Expected: v1 = (4.0, 3.0)
      dcomplex val1(4.0, 3.0);
      edge_ops->setToScalar(svindx[1], dcomplex(4.0, 3.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!complexDataSameAsValue(svindx[1], val1, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #3b: math::HierarchyEdgeDataOpsComplex::setToScalar()\n"
         << "Expected: v1 = " << val1 << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #4: math::HierarchyEdgeDataOpsComplex::copyData()
      // Expected: v2 = v1 = (4.0, 3.0)
      edge_ops->copyData(svindx[2], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!complexDataSameAsValue(svindx[2], val1, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #4: math::HierarchyEdgeDataOpsComplex::copyData()\n"
         << "Expected: v2 = " << val1 << std::endl;
         edge_ops->printData(svindx[2], tbox::plog);
      }

      // Test #5: math::HierarchyEdgeDataOpsComplex::swapData()
      // Expected:  v0 = (4.0, 3.0), v1 = (2.0,1.5)
      edge_ops->swapData(svindx[0], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!complexDataSameAsValue(svindx[0], val1, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #5a: math::HierarchyEdgeDataOpsComplex::swapData()\n"
         << "Expected: v0 = " << val1 << std::endl;
         edge_ops->printData(svindx[0], tbox::plog);
      }
      if (!complexDataSameAsValue(svindx[1], val0, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #5b: math::HierarchyEdgeDataOpsComplex::swapData()\n"
         << "Expected: v1 = " << val0 << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #6: math::HierarchyEdgeDataOpsComplex::scale()
      // Expected:  v2 = 0.25 * v2 = (1.0,0.75)
      edge_ops->scale(svindx[2], 0.25, svindx[2]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_scale(1.0, 0.75);
      if (!complexDataSameAsValue(svindx[2], val_scale, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #6: math::HierarchyEdgeDataOpsComplex::scale()\n"
         << "Expected: v2 = " << val_scale << std::endl;
         edge_ops->printData(svindx[2], tbox::plog);
      }

      // Test #7: math::HierarchyEdgeDataOpsComplex::add()
      // Expected:  v3 = v0 + v1 = (6.0, 4.5)
      edge_ops->add(svindx[3], svindx[0], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_add(6.0, 4.5);
      if (!complexDataSameAsValue(svindx[3], val_add, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #7: math::HierarchyEdgeDataOpsComplex::add()\n"
         << "Expected: v3 = " << val_add << std::endl;
         edge_ops->printData(svindx[3], tbox::plog);
      }

      // Reset v0: v0 = (0.0,4.5)
      edge_ops->setToScalar(svindx[0], dcomplex(0.0, 4.5));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif

      // Test #8: math::HierarchyEdgeDataOpsComplex::subtract()
      // Expected:  v1 = v3 - v0 = (6.0,0.0)
      edge_ops->subtract(svindx[1], svindx[3], svindx[0]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_sub(6.0, 0.0);
      if (!complexDataSameAsValue(svindx[1], val_sub, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #8: math::HierarchyEdgeDataOpsComplex::subtract()\n"
         << "Expected: v1 = " << val_sub << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #9a: math::HierarchyEdgeDataOpsComplex::addScalar()
      // Expected:  v1 = v1 + (0.0,-4.0) = (6.0,-4.0)
      edge_ops->addScalar(svindx[1], svindx[1], dcomplex(0.0, -4.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_addScalar(6.0, -4.0);
      if (!complexDataSameAsValue(svindx[1], val_addScalar, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #9a: math::HierarchyEdgeDataOpsComplex::addScalar()\n"
         << "Expected: v1 = " << val_addScalar << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #9b: math::HierarchyEdgeDataOpsComplex::addScalar()
      // Expected:  v2 = v2 + (0.0,0.25) = (1.0,1.0)
      edge_ops->addScalar(svindx[2], svindx[2], dcomplex(0.0, 0.25));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      val_addScalar = dcomplex(1.0, 1.0);
      if (!complexDataSameAsValue(svindx[2], val_addScalar, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #9b: math::HierarchyEdgeDataOpsComplex::addScalar()\n"
         << "Expected: v2 = " << val_addScalar << std::endl;
         edge_ops->printData(svindx[2], tbox::plog);
      }

      // Test #9c: math::HierarchyEdgeDataOpsComplex::addScalar()
      // Expected:  v2 = v2 + (3.0,-4.0) = (4.0,-3.0)
      edge_ops->addScalar(svindx[2], svindx[2], dcomplex(3.0, -4.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      val_addScalar = dcomplex(4.0, -3.0);
      if (!complexDataSameAsValue(svindx[2], val_addScalar, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #9c: math::HierarchyEdgeDataOpsComplex::addScalar()\n"
         << "Expected: v2 = " << val_addScalar << std::endl;
         edge_ops->printData(svindx[2], tbox::plog);
      }

      // Rest v3:  v3 = (0.5, 0.0)
      edge_ops->setToScalar(svindx[3], dcomplex(0.5, 0.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif

      // Test #10: math::HierarchyEdgeDataOpsComplex::multiply()
      // Expected:  v1 = v3 * v1 = (3.0,-2.0)
      edge_ops->multiply(svindx[1], svindx[3], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_mult(3.0, -2.0);
      if (!complexDataSameAsValue(svindx[1], val_mult, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #10: math::HierarchyEdgeDataOpsComplex::multiply()\n"
         << "Expected: v1 = " << val_mult << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #11: math::HierarchyEdgeDataOpsComplex::divide()
      // Expected:  v0 = v2 / v1 = (1.3846153846154,-0.076923076923077)
      edge_ops->divide(svindx[0], svindx[2], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_div(1.3846153846154, -0.076923076923077);
      if (!complexDataSameAsValue(svindx[0], val_div, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #11: math::HierarchyEdgeDataOpsComplex::divide()\n"
         << "Expected: v0 = " << val_div << std::endl;
         edge_ops->printData(svindx[0], tbox::plog);
      }

      // Test #12: math::HierarchyEdgeDataOpsComplex::reciprocal()
      // Expected:  v1 = 1 / v1 = (0.23076923076923, 0.15384615384615)
      edge_ops->reciprocal(svindx[1], svindx[1]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_rec(0.23076923076923, 0.15384615384615);
      if (!complexDataSameAsValue(svindx[1], val_rec, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #12: math::HierarchyEdgeDataOpsComplex::reciprocal()\n"
         << "Expected: v1 = " << val_rec << std::endl;
         edge_ops->printData(svindx[1], tbox::plog);
      }

      // Test #13:  Place some bogus values on coarse level
      std::shared_ptr<pdat::EdgeData<dcomplex> > sdata;

      // set values

      std::shared_ptr<hier::PatchLevel> level_zero(
         hierarchy->getPatchLevel(0));
      for (hier::PatchLevel::iterator ip(level_zero->begin());
           ip != level_zero->end(); ++ip) {
         patch = *ip;
         sdata = SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<dcomplex>,
                            hier::PatchData>(patch->getPatchData(svindx[2]));
         TBOX_ASSERT(sdata);
         hier::Index index0(dim, 2);
         hier::Index index1(dim, 3);
         index1(0) = 5;
         if (dim.getValue() == 2) {
            if (patch->getBox().contains(index0)) {
               (*sdata)(pdat::EdgeIndex(index0, pdat::EdgeIndex::Y,
                           pdat::EdgeIndex::Lower), 0) = dcomplex(100.0, -50.0);
            }
            if (patch->getBox().contains(index1)) {
               (*sdata)(pdat::EdgeIndex(index1, pdat::EdgeIndex::Y,
                           pdat::EdgeIndex::Upper), 0) = dcomplex(-1000.0, 20.0);
            }
         } else {
            if (patch->getBox().contains(index0)) {
               (*sdata)(pdat::EdgeIndex(index0, pdat::EdgeIndex::Y,
                           pdat::EdgeIndex::LowerRight), 0) = dcomplex(100.0, -50.0);
            }
            if (patch->getBox().contains(index1)) {
               (*sdata)(pdat::EdgeIndex(index1, pdat::EdgeIndex::Y,
                           pdat::EdgeIndex::UpperRight), 0) = dcomplex(-1000.0, 20.0);
            }
         }
      }

      // check values
      bool bogus_value_test_passed = true;
      for (hier::PatchLevel::iterator ipp(level_zero->begin());
           ipp != level_zero->end(); ++ipp) {
         patch = *ipp;
         sdata = SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<dcomplex>,
                            hier::PatchData>(patch->getPatchData(svindx[2]));
         TBOX_ASSERT(sdata);
         hier::Index idx0(dim, 2);
         hier::Index idx1(dim, 3);
         idx1(0) = 5;
         int corner0, corner1;
         if (dim.getValue() == 2) {
            corner0 = pdat::EdgeIndex::Lower;
            corner1 = pdat::EdgeIndex::Upper;
         } else {
            corner0 = pdat::EdgeIndex::LowerRight;
            corner1 = pdat::EdgeIndex::UpperRight;
         }
         pdat::EdgeIndex index0(idx0, pdat::EdgeIndex::Y, corner0);
         pdat::EdgeIndex index1(idx1, pdat::EdgeIndex::Y, corner1);

         // check X axis data
         pdat::EdgeIterator cend(pdat::EdgeGeometry::end(sdata->getBox(), pdat::EdgeIndex::X));
         for (pdat::EdgeIterator c(pdat::EdgeGeometry::begin(sdata->getBox(), pdat::EdgeIndex::X));
              c != cend && bogus_value_test_passed; ++c) {
            pdat::EdgeIndex edge_index = *c;

            if (!tbox::MathUtilities<dcomplex>::equalEps((*sdata)(edge_index),
                   dcomplex(4.0, -3.0))) {
               bogus_value_test_passed = false;
            }
         }

         // check Y axis data
         pdat::EdgeIterator ccend(pdat::EdgeGeometry::end(sdata->getBox(), pdat::EdgeIndex::Y));
         for (pdat::EdgeIterator cc(pdat::EdgeGeometry::begin(sdata->getBox(), pdat::EdgeIndex::Y));
              cc != ccend && bogus_value_test_passed; ++cc) {
            pdat::EdgeIndex edge_index = *cc;

            if (edge_index == index0) {
               if (!tbox::MathUtilities<dcomplex>::equalEps((*sdata)(edge_index),
                      dcomplex(100.0, -50.0))) {
                  bogus_value_test_passed = false;
               }
            } else {
               if (edge_index == index1) {
                  if (!tbox::MathUtilities<dcomplex>::equalEps((*sdata)(
                            edge_index),
                         dcomplex(-1000.0, 20.0))) {
                     bogus_value_test_passed = false;
                  }
               } else {
                  if (!tbox::MathUtilities<dcomplex>::equalEps((*sdata)(
                            edge_index),
                         dcomplex(4.0, -3.0))) {
                     bogus_value_test_passed = false;
                  }
               }
            }
         }

         if (dim.getValue() == 3) {
            // check Z axis data
            pdat::EdgeIterator cend(pdat::EdgeGeometry::end(sdata->getBox(), pdat::EdgeIndex::Z));
            for (pdat::EdgeIterator c(pdat::EdgeGeometry::begin(sdata->getBox(), pdat::EdgeIndex::Z));
                 c != cend && bogus_value_test_passed; ++c) {
               pdat::EdgeIndex edge_index = *c;

               if (!tbox::MathUtilities<dcomplex>::equalEps((*sdata)(edge_index),
                      dcomplex(4.0, -3.0))) {
                  bogus_value_test_passed = false;
               }
            }
         }
      }
      if (!bogus_value_test_passed) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #13:  Place some bogus values on coarse level"
         << std::endl;
         edge_ops->printData(svindx[2], tbox::plog);
      }

      // Test norms on patch data with svindx[2] on hierarchy with bogus values

      // Test #14: math::HierarchyEdgeDataOpsComplex::L1Norm() - w/o control weight
      // Expected:  bogus_l1_norm = 2217.003379
      double bogus_l1_norm = edge_ops->L1Norm(svindx[2]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      {
         double compare;
         if (dim.getValue() == 2) {
            compare = 2217.003379;
         } else {
            compare = 15477.0033789;
         }
         if (!tbox::MathUtilities<double>::equalEps(bogus_l1_norm, compare)) {
            ++num_failures;
            tbox::perr
            << "FAILED: - Test #14: math::HierarchyEdgeDataOpsComplex::L1Norm()"
            << " - w/o control weight\n"
            << "Expected value = " << compare << ", Computed value = "
            << std::setprecision(12) << bogus_l1_norm << std::endl;
         }
      }

      // Test #15: math::HierarchyEdgeDataOpsComplex::L1Norm() - w/control weight
      // Expected:  correct_l1_norm = 5.0
      double correct_l1_norm = edge_ops->L1Norm(svindx[2], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      {
         double compare;
         if (dim.getValue() == 2) {
            compare = 5.0;
         } else {
            compare = 7.5;
         }
         if (!tbox::MathUtilities<double>::equalEps(correct_l1_norm, compare)) {
            ++num_failures;
            tbox::perr
            << "FAILED: - Test #15: math::HierarchyEdgeDataOpsComplex::L1Norm()"
            << " - w/control weight\n"
            << "Expected value = " << compare << ", Computed value = "
            << correct_l1_norm << std::endl;
         }
      }

      // Test #16: math::HierarchyEdgeDataOpsComplex::L2Norm()
      // Expected:  l2_norm = 5.0
      double l2_norm = edge_ops->L2Norm(svindx[2], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      {
         double compare;
         if (dim.getValue() == 2) {
            compare = 5.0;
         } else {
            compare = 6.12372435696;
         }
         if (!tbox::MathUtilities<double>::equalEps(l2_norm, compare)) {
            ++num_failures;
            tbox::perr
            << "FAILED: - Test #16: math::HierarchyEdgeDataOpsComplex::L2Norm()\n"
            << "Expected value = " << compare << ", Computed value = "
            << l2_norm << std::endl;
         }
      }

      // Test #17: math::HierarchyEdgeDataOpsComplex::maxNorm() - w/o control weight
      // Expected:  bogus_max_norm = 1000.19998
      double bogus_max_norm = edge_ops->maxNorm(svindx[2]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!tbox::MathUtilities<double>::equalEps(bogus_max_norm, 1000.19998)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #17: math::HierarchyEdgeDataOpsComplex::maxNorm()"
         << " - w/o control weight\n"
         << "Expected value = 1000.19998, Computed value = "
         << bogus_max_norm << std::endl;
      }

      // Test #18: math::HierarchyEdgeDataOpsComplex::maxNorm() - w/control weight
      // Expected:  max_norm = 5.0
      double max_norm = edge_ops->maxNorm(svindx[2], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!tbox::MathUtilities<double>::equalEps(max_norm, 5.0)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #18: math::HierarchyEdgeDataOpsComplex::maxNorm()"
         << " - w/control weight\n"
         << "Expected value = 5.0, Computed value = "
         << max_norm << std::endl;
      }

      // Reset data and test sums, axpy's
      edge_ops->setToScalar(svindx[0], dcomplex(1.0, -3.0));
      edge_ops->setToScalar(svindx[1], dcomplex(2.5, 3.0));
      edge_ops->setToScalar(svindx[2], dcomplex(7.0, 0.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif

      // Test #19: math::HierarchyEdgeDataOpsComplex::linearSum()
      // Expected:  v3 = (2.0,5.0)
      edge_ops->linearSum(svindx[3],
         dcomplex(2.0, 0.0), svindx[1], dcomplex(0.0, -1.0), svindx[0]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_linearSum(2.0, 5.0);
      if (!complexDataSameAsValue(svindx[3], val_linearSum, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #19: math::HierarchyEdgeDataOpsComplex::linearSum()\n"
         << "Expected: v3 = " << val_linearSum << std::endl;
         edge_ops->printData(svindx[3], tbox::plog);
      }

      // Test #20: math::HierarchyEdgeDataOpsComplex::axmy()
      // Expected:  v3 = (6.5,12.0)
      edge_ops->axmy(svindx[3], 3.0, svindx[1], svindx[0]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex val_axmy(6.5, 12.0);
      if (!complexDataSameAsValue(svindx[3], val_axmy, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #20: math::HierarchyEdgeDataOpsComplex::axmy()\n"
         << "Expected: v3 = " << val_axmy << std::endl;
         edge_ops->printData(svindx[3], tbox::plog);
      }

      // Test #21a: math::HierarchyEdgeDataOpsComplex::dot() - (ind2) * (ind1)
      // Expected:  cdot = (17.5,-21.0)
      dcomplex cdot = edge_ops->dot(svindx[2], svindx[1], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      double ctl_wt;
      if (dim.getValue() == 2) {
         ctl_wt = 1.0;
      } else {
         ctl_wt = 1.5;
      }
      dcomplex ans_2_dot_1(17.5 * ctl_wt, -21.0 * ctl_wt);
      if (!tbox::MathUtilities<dcomplex>::equalEps(cdot, ans_2_dot_1)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #21a: math::HierarchyEdgeDataOpsComplex::dot() - (ind2) * (ind1)\n"
         << "Expected value = " << ans_2_dot_1 << ", Computed value = "
         << cdot << std::endl;
      }

      // Test #21b: math::HierarchyEdgeDataOpsComplex::dot() - (ind1) * (ind2)
      // Expected:  cdot = (17.5,-1.0)
      dcomplex cdot2 = edge_ops->dot(svindx[1], svindx[2], swgt_id);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      dcomplex ans_1_dot_2(17.5 * ctl_wt, 21.0 * ctl_wt);
      if (!tbox::MathUtilities<dcomplex>::equalEps(cdot2, ans_1_dot_2)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #21b: math::HierarchyEdgeDataOpsComplex::dot() - (ind1) * (ind2)\n"
         << "Expected value = " << ans_1_dot_2 << ", Computed value = "
         << cdot2 << std::endl;
      }

      // Test #22: math::HierarchyEdgeDataOpsComplex::abs()
      // Expected:  abs(v0) = 5.0
      edge_ops->setToScalar(svindx[0], dcomplex(4.0, -3.0));
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      edge_ops->abs(swgt_id, svindx[0]);
#if defined(HAVE_RAJA)
      tbox::parallel_synchronize();
#endif
      if (!doubleDataSameAsValue(swgt_id, 5.0, hierarchy)) {
         ++num_failures;
         tbox::perr
         << "FAILED: - Test #22: math::HierarchyEdgeDataOpsComplex::abs()\n"
         << "Expected: abs(v0) = 5.0" << std::endl;
         swgt_ops->printData(swgt_id, tbox::plog);
      }

      // deallocate data on hierarchy
      for (ln = 0; ln < 2; ++ln) {
         hierarchy->getPatchLevel(ln)->deallocatePatchData(swgt_id);
         for (iv = 0; iv < NVARS; ++iv) {
            hierarchy->getPatchLevel(ln)->deallocatePatchData(svindx[iv]);
         }
      }

      for (iv = 0; iv < NVARS; ++iv) {
         fvar[iv].reset();
      }
      swgt.reset();

      geometry.reset();
      hierarchy.reset();
      edge_ops.reset();
      swgt_ops.reset();

      if (num_failures == 0) {
         tbox::pout << "\nPASSED:  edge cplxtest" << std::endl;
      }
   }

   tbox::SAMRAIManager::shutdown();
   tbox::SAMRAIManager::finalize();
   tbox::SAMRAI_MPI::finalize();

   return num_failures;
}

/*
 * Returns true if all the data in the hierarchy is equal to the specified
 * value.  Returns false otherwise.
 */
static bool
complexDataSameAsValue(
   int desc_id,
   dcomplex value,
   std::shared_ptr<hier::PatchHierarchy> hierarchy)
{
   bool test_passed = true;

   int ln;
   std::shared_ptr<hier::Patch> patch;
   for (ln = 0; ln < 2; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         patch = *ip;
         std::shared_ptr<pdat::EdgeData<dcomplex> > svdata(
            SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<dcomplex>, hier::PatchData>(
               patch->getPatchData(desc_id)));

         TBOX_ASSERT(svdata);

         pdat::EdgeIterator cend(pdat::EdgeGeometry::end(svdata->getBox(), 1));
         for (pdat::EdgeIterator c(pdat::EdgeGeometry::begin(svdata->getBox(), 1));
              c != cend && test_passed; ++c) {
            pdat::EdgeIndex edge_index = *c;
            if (!tbox::MathUtilities<dcomplex>::equalEps((*svdata)(edge_index),
                   value)) {
               test_passed = false;
            }
         }
      }
   }

   return test_passed;
}

/*
 * Returns true if all the data in the hierarchy is equal to the specified
 * value.  Returns false otherwise.
 */
static bool
doubleDataSameAsValue(
   int desc_id,
   double value,
   std::shared_ptr<hier::PatchHierarchy> hierarchy)
{
   bool test_passed = true;

   int ln;
   std::shared_ptr<hier::Patch> patch;
   for (ln = 0; ln < 2; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         patch = *ip;
         std::shared_ptr<pdat::EdgeData<double> > svdata(
            SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<double>, hier::PatchData>(
               patch->getPatchData(desc_id)));

         TBOX_ASSERT(svdata);

         pdat::EdgeIterator cend(pdat::EdgeGeometry::end(svdata->getBox(), 1));
         for (pdat::EdgeIterator c(pdat::EdgeGeometry::begin(svdata->getBox(), 1));
              c != cend && test_passed; ++c) {
            pdat::EdgeIndex edge_index = *c;
            if (!tbox::MathUtilities<double>::equalEps((*svdata)(edge_index),
                   value)) {
               test_passed = false;
            }
         }
      }
   }

   return test_passed;
}
