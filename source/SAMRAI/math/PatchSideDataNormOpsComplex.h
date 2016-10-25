/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and COPYING.LESSER.
 *
 * Copyright:     (c) 1997-2016 Lawrence Livermore National Security, LLC
 * Description:   Norm operations for complex side-centered data.
 *
 ************************************************************************/

#ifndef included_math_PatchSideDataNormOpsComplex
#define included_math_PatchSideDataNormOpsComplex

#include "SAMRAI/SAMRAI_config.h"

#include "SAMRAI/pdat/SideData.h"
#include "SAMRAI/math/ArrayDataNormOpsComplex.h"
#include "SAMRAI/hier/Box.h"
#include "SAMRAI/tbox/Complex.h"
#include "SAMRAI/tbox/Utilities.h"


namespace SAMRAI {
namespace math {

/**
 * Class PatchSideDataNormOpsComplex provides a collection of common
 * norm operations that may be applied to complex side-centered patch data.
 * The primary intent of this class is to define part of the interface for the
 * PatchSideDataOpsComplex class which provides access to operations
 * that may be used to manipulate complex side-centered patch data.  Each
 * member function accepts a box argument indicating the region of index space
 * on which the operation should be performed.  The operation will be performed
 * on the intersection of this box and those boxes corresponding to the patch
 * data objects.  Also, each operation allows an additional side-centered
 * patch data object to be used to represent a control volume that weights the
 * contribution of each data entry in the given norm calculation.  Note that
 * the control volume patch data must be of type double and have side-centered
 * geometry (i.e., the same as the data itself).  The use of control volumes
 * is important when these operations are used in vector kernels where the
 * data resides over multiple levels of spatial resolution in an AMR hierarchy.
 * If the control volume is not given in the function call, it will be ignored
 * in the calculation.  Also, note that the depth of the control volume patch
 * data object must be either 1 or be equal to the depth of the other data
 * objects.
 *
 * Note that a similar set of norm operations is implemented for real
 * patch data (double and float) in the class PatchSideDataNormOpsReal.
 *
 * @see ArrayDataNormOpsComplex
 */

class PatchSideDataNormOpsComplex
{
public:
   /**
    * Empty constructor and destructor.
    */
   PatchSideDataNormOpsComplex();

   virtual ~PatchSideDataNormOpsComplex();

   /**
    * Return the number of data values for the side-centered data object
    * in the given box.  Note that it is assumed that the box refers to
    * the cell-centered index space corresponding to the patch hierarchy.
    *
    * @pre data
    * @pre data->getDim() == box.getDim()
    */
   int
   numberOfEntries(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box) const;

   /**
    * Return sum of control volume entries for the side-centered data object.
    *
    * @pre data && cvol
    * @pre data->getDirectionVector() == hier::IntVector::min(data->getDirectionVector(), cvol->getDirectionVector())
    */
   double
   sumControlVolumes(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const std::shared_ptr<pdat::SideData<double> >& cvol,
      const hier::Box& box) const;

   /**
    * Set destination component to norm of source component.  That is,
    * each destination entry is set to
    * \f$d_i = \sqrt{ {real(s_i)}^2 + {imag(s_i)}^2 }\f$.
    *
    * @pre dst && src
    * @pre dst->getDirectionVector() == src->getDirectionVector()
    * @pre (dst->getDim() == src->getDim()) && (dst->getDim() == box.getDim())
    */
   void
   abs(
      const std::shared_ptr<pdat::SideData<double> >& dst,
      const std::shared_ptr<pdat::SideData<dcomplex> >& src,
      const hier::Box& box) const;

   /**
    * Return discrete \f$L_1\f$-norm of the data using the control volume to
    * weight the contribution of each data entry to the sum.  That is, the
    * return value is the sum \f$\sum_i ( \sqrt{data_i * \bar{data_i}}*cvol_i )\f$.
    * If the control volume is NULL, the return value is
    * \f$\sum_i ( \sqrt{data_i * \bar{data_i}} )\f$.
    *
    * @pre data
    * @pre data->getDim() == box.getDim()
    * @pre !cvol || (data->getDirectionVector() ==  hier::IntVector::min(data->getDirectionVector(), cvol->getDirectionVector()))
    * @pre !cvol || (data->getDim() == cvol->getDim())
    */
   double
   L1Norm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return discrete \f$L_2\f$-norm of the data using the control volume to
    * weight the contribution of each data entry to the sum.  That is, the
    * return value is the sum
    * \f$\sqrt{ \sum_i ( data_i * \bar{data_i} cvol_i ) }\f$.
    * If the control volume is NULL, the return value is
    * \f$\sqrt{ \sum_i ( data_i * \bar{data_i} ) }\f$.
    *
    * @pre data
    * @pre data->getDim() == box.getDim()
    * @pre !cvol || (data->getDirectionVector() ==  hier::IntVector::min(data->getDirectionVector(), cvol->getDirectionVector()))
    * @pre !cvol || (data->getDim() == cvol->getDim())
    */
   double
   L2Norm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return discrete weighted \f$L_2\f$-norm of the data using the control
    * volume to weight the contribution of the data and weight entries to
    * the sum.  That is, the return value is the sum \f$\sqrt{ \sum_i (
    * (data_i * wgt_i) * \bar{(data_i * wgt_i)} cvol_i ) }\f$.  If the control
    * volume is NULL, the return value is
    * \f$\sqrt{ \sum_i ( (data_i * wgt_i) * \bar{(data_i * wgt_i)} cvol_i ) }\f$.
    *
    * @pre data && weight
    * @pre (data->getDim() == weight->getDim()) &&
    *      (data->getDim() == box.getDim())
    * @pre data->getDirectionVector() ==  hier::IntVector::min(data->getDirectionVector(), weight->getDirectionVector())
    * @pre !cvol || (data->getDirectionVector() ==  hier::IntVector::min(data->getDirectionVector(), cvol->getDirectionVector()))
    * @pre !cvol || (data->getDim() == cvol->getDim())
    */
   double
   weightedL2Norm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const std::shared_ptr<pdat::SideData<dcomplex> >& weight,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return discrete root mean squared norm of the data.  If the control
    * volume is not NULL, the return value is the \f$L_2\f$-norm divided by
    * the square root of the sum of the control volumes.  Otherwise, the
    * return value is the \f$L_2\f$-norm divided by the square root of the
    * number of data entries.
    *
    * @pre data
    */
   double
   RMSNorm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return discrete weighted root mean squared norm of the data.  If the
    * control volume is not NULL, the return value is the weighted \f$L_2\f$-norm
    * divided by the square root of the sum of the control volumes.  Otherwise,
    * the return value is the weighted \f$L_2\f$-norm divided by the square
    * root of the number of data entries.
    *
    * @pre data && weight
    */
   double
   weightedRMSNorm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const std::shared_ptr<pdat::SideData<dcomplex> >& weight,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return the \f$\max\f$-norm of the data using the control volume to weight
    * the contribution of each data entry to the maximum.  That is, the return
    * value is \f$\max_i ( \sqrt{data_i * \bar{data_i}} )\f$, where the max is
    * over the data elements where \f$cvol_i > 0\f$.  If the control volume is
    * NULL, it is ignored during the computation of the maximum.
    *
    * @pre data
    * @pre !cvol || (data->getDirectionVector() == hier::IntVector::min(data->getDirectionVector(), cvol->getDirectionVector()))
    */
   double
   maxNorm(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return the dot product of the two data arrays using the control volume
    * to weight the contribution of each product to the sum.  That is, the
    * return value is the sum \f$\sum_i ( data1_i * \bar{data2_i} * cvol_i )\f$.
    * If the control volume is NULL, it is ignored during the summation.
    *
    * @pre data1 && data2
    * @pre data1->getDirectionVector() == data2->getDirectionVector()
    * @pre !cvol || (data1->getDirectionVector() == hier::IntVector::min(data1->getDirectionVector(), cvol->getDirectionVector()))
    */
   dcomplex
   dot(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data1,
      const std::shared_ptr<pdat::SideData<dcomplex> >& data2,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& cvol =
         std::shared_ptr<pdat::SideData<double> >()) const;

   /**
    * Return the integral of the function represented by the data array.
    * The return value is the sum \f$\sum_i ( data_i * vol_i )\f$.
    *
    * @pre data
    * @pre data->getDirectionVector() == hier::IntVector::min(data->getDirectionVector(), vol->getDirectionVector())
    */
   dcomplex
   integral(
      const std::shared_ptr<pdat::SideData<dcomplex> >& data,
      const hier::Box& box,
      const std::shared_ptr<pdat::SideData<double> >& vol) const;

private:
   // The following are not implemented:
   PatchSideDataNormOpsComplex(
      const PatchSideDataNormOpsComplex&);
   PatchSideDataNormOpsComplex&
   operator = (
      const PatchSideDataNormOpsComplex&);

   ArrayDataNormOpsComplex d_array_ops;
};

}
}

#endif
