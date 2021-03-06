/***************************************************************************
 * libRSF - A Robust Sensor Fusion Library
 *
 * Copyright (C) 2018 Chair of Automation Technology / TU Chemnitz
 * For more information see https://www.tu-chemnitz.de/etit/proaut/libRSF
 *
 * libRSF is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libRSF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libRSF.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Tim Pfeifer (tim.pfeifer@etit.tu-chemnitz.de)
 ***************************************************************************/

/**
 * @file SumMixture.h
 * @author Tim Pfeifer
 * @date 18.09.2018
 * @brief Sum-Mixture error model inspired from the work of Rosen.
 * @copyright GNU Public License.
 *
 */

#ifndef SUMMIXTURE_H
#define SUMMIXTURE_H

#include "ErrorModel.h"
#include "GaussianMixture.h"
#include "../NumericalRobust.h"

namespace libRSF
{
  /** \brief The robust Sum-Mixture error model
   * Based on:
   * D. M. Rosen, M. Kaess, and J. J. Leonard
   * “Robust incremental online inference over sparse factor graphs: Beyond the Gaussian case”
   * Proc. of Intl. Conf. on Robotics and Automation (ICRA), Karlsruhe, 2013.
   * DOI: 10.1109/ICRA.2013.6630699
   *
   * \param Mixture Underlying mixture distribution
   *
   */
  template <int Dim, typename MixtureType, bool SpecialNormalization>
  class SumMixture : public ErrorModel <Dim, Dim>
  {
  public:
    SumMixture()
    {
      _Normalization = 0;
    }

    virtual ~SumMixture() = default;

    explicit SumMixture(const MixtureType &Mixture)
    {
      this->addMixture(Mixture);
    }

    void clear()
    {
      _Normalization = 0;
      _Mixture.clear();
    }

    template <typename T>
    bool weight(const VectorT<T, Dim> &RawError, T* Error) const
    {
      /** map error to eigen matrix for easier access */
      VectorRef<T, Dim> ErrorMap(Error);

      if(this->_Enable)
      {
        const int NumberOfComponents = _Mixture.getNumberOfComponents();

        MatrixT<T, Dynamic, 1> Scalings(NumberOfComponents);
        MatrixT<T, Dynamic, 1> Exponents(NumberOfComponents);

        /** calculate all exponents and scalings */
        for(int nComponent = 0; nComponent < NumberOfComponents; ++nComponent)
        {
          Exponents(nComponent) = - 0.5 * (_Mixture.template getExponentialPartOfComponent<T>(nComponent, RawError).squaredNorm() + 1e-10);
          Scalings(nComponent) = T(_Mixture.template getLinearPartOfComponent<T>(nComponent, RawError));
        }

        /** combine them numerically robust and distribute the error equally over all dimensions */
        ErrorMap.fill(sqrt(-2.0* (ScaledLogSumExp(Exponents, Scalings) - log(_Normalization + 1e-10))) / sqrt(Dim));
      }
      else
      {
        /** pass raw error trough */
        VectorRef<T, Dim> ErrorMap(Error);
        ErrorMap = RawError;
      }

      return true;
    }

  private:

    void addMixture(const MixtureType &Mixture)
    {
      _Mixture = Mixture;

      const int NumberOfComponents = _Mixture.getNumberOfComponents();

      if constexpr(SpecialNormalization == false)
      {
        /** original version */
        _Normalization = 0;
        for(int nComponent = 0; nComponent < NumberOfComponents; ++nComponent)
        {
          _Normalization += _Mixture.getMaximumOfComponent(nComponent);
        }
      }
      else
      {
        /** version for Reviewer 3 */
        _Normalization = _Mixture.getMaximumOfComponent(0);
        for(int nComponent = 1; nComponent < NumberOfComponents; ++nComponent)
        {
          _Normalization = std::max(_Normalization, _Mixture.getMaximumOfComponent(nComponent));
        }
        _Normalization = _Normalization*NumberOfComponents + 10;
      }
    }

    MixtureType _Mixture;
    double _Normalization;
  };

  typedef SumMixture<1, GaussianMixture<1>, false> SumMix1;
  typedef SumMixture<2, GaussianMixture<2>, false> SumMix2;
  typedef SumMixture<3, GaussianMixture<3>, false> SumMix3;

  typedef SumMixture<1, GaussianMixture<1>, true> SumMix1Special;
  typedef SumMixture<2, GaussianMixture<2>, true> SumMix2Special;
  typedef SumMixture<3, GaussianMixture<3>, true> SumMix3Special;
}

#endif // SUMMIXTURE_H
