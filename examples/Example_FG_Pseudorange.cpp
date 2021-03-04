/***************************************************************************
 * libRSF - A Robust Sensor Fusion Library
 *
 * Copyright (C) 2018 Chair of Automation Technology / TU Chemnitz
 * For more information see https://www.tu-chemnitz.de/etit/proaut/self-tuning
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
* @file Example_FG_Pseudorange.cpp
* @author Tim Pfeifer
* @date 27.07.2016
* @brief An example application to demonstrate the construction of a simple factor graph with pseudo range measurements.
* @copyright GNU Public License.
*
*/

#include "Example_FG_Pseudorange.h"

void CreateData (libRSF::SensorDataSet &RangeMeasurements)
{
  std::default_random_engine Generator;
  std::normal_distribution<double> Distribution(0.0, STDDEV_RANGE);

  libRSF::Vector1 Range, StdDev, SatID;
  vector<libRSF::Vector2> SatPositions;
  vector<libRSF::Vector2> EgoPositions;

  StdDev << STDDEV_RANGE;

  SatPositions.push_back((libRSF::Vector2() << 10, 10).finished());
  SatPositions.push_back((libRSF::Vector2() << 10, -10).finished());
  SatPositions.push_back((libRSF::Vector2() << -10, 10).finished());
  SatPositions.push_back((libRSF::Vector2() << -10, -10).finished());

  EgoPositions.push_back((libRSF::Vector2() << 0, 0).finished());
  EgoPositions.push_back((libRSF::Vector2() << 1, 0).finished());
  EgoPositions.push_back((libRSF::Vector2() << 1, 1).finished());

  for (int i = 0; i < static_cast<int>(EgoPositions.size()); i++)
  {
    for (int j = 0; j < static_cast<int>(SatPositions.size()); j++)
    {
      Range[0] = (SatPositions.at(j) - EgoPositions.at(i)).norm() + Distribution(Generator) + OFFSET;
      SatID << j;

      libRSF::SensorData PseudorangeMasurement(libRSF::SensorType::Pseudorange2, i);
      PseudorangeMasurement.setMean(Range);
      PseudorangeMasurement.setStdDev(StdDev);
      PseudorangeMasurement.setValue(libRSF::SensorElement::SatPos, SatPositions.at(j));
      PseudorangeMasurement.setValue(libRSF::SensorElement::SatID, SatID);

      RangeMeasurements.addElement(PseudorangeMasurement);
    }
  }
}

#ifndef TESTMODE // only compile main if not used in test context

int main(int ArgC, char** ArgV)
{
  (void)ArgC;
  google::InitGoogleLogging(ArgV[0]);

  double Time, TimeFirst = 0.0, TimeOld = 0.0;

  libRSF::Vector1 StdDev, StdDevCCE;
  StdDev << STDDEV_RANGE;
  StdDevCCE << STDDEV_CCE;

  ceres::Solver::Options SolverOptions;
  SolverOptions.trust_region_strategy_type = ceres::TrustRegionStrategyType::DOGLEG;
  SolverOptions.dogleg_type = ceres::DoglegType::SUBSPACE_DOGLEG;
  SolverOptions.num_threads = std::thread::hardware_concurrency();
  SolverOptions.minimizer_progress_to_stdout = true;

  libRSF::FactorGraph SimpleGraph;
  libRSF::SensorDataSet RangeMeasurements;
  libRSF::StateList RangeList, ConstValList;
  libRSF::GaussianDiagonal<1> NoiseModelRange, NoiseModelCCE;
  NoiseModelRange.setStdDevDiagonal(StdDev);
  NoiseModelCCE.setStdDevDiagonal(StdDevCCE);

  /** construct a set of range Measurements */
  CreateData(RangeMeasurements);

  /** loop over timestamps */
  RangeMeasurements.getTimeFirst(PSEUDORANGE_MEASUREMENT, TimeFirst);
  Time = TimeFirst;
  do
  {
    /** add position variables to graph */
    SimpleGraph.addState(POSITION_STATE, libRSF::StateType::Point2, Time);

    /** add offset variables to graph */
    SimpleGraph.addState(OFFSET_STATE, libRSF::StateType::ClockError, Time);

    RangeList.add(POSITION_STATE, Time);
    RangeList.add(OFFSET_STATE, Time);

    /** add constant value model*/
    if (Time > TimeFirst)
    {
      ConstValList.add(OFFSET_STATE, TimeOld);
      ConstValList.add(OFFSET_STATE, Time);
      SimpleGraph.addFactor<libRSF::FactorType::ConstVal1>(ConstValList, NoiseModelCCE);
      ConstValList.clear();
    }

    /** loop over measurements */
    for (int nMeasurement = RangeMeasurements.countElement(PSEUDORANGE_MEASUREMENT, Time); nMeasurement > 0; nMeasurement--)
    {
      SimpleGraph.addFactor<libRSF::FactorType::Pseudorange2>(RangeList, RangeMeasurements.getElement(PSEUDORANGE_MEASUREMENT, Time, nMeasurement-1), NoiseModelRange);
    }
    RangeList.clear();

    TimeOld = Time;
  }
  while(RangeMeasurements.getTimeNext(PSEUDORANGE_MEASUREMENT, Time, Time));

  /** output initialization */
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 0.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 1.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 2.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 0.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 1.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 2.0).getNameValueString() << std::endl <<std::endl;

  /** solve graph */
  SimpleGraph.solve(SolverOptions);
  SimpleGraph.printReport();

  /** calculate covariance */
  SimpleGraph.computeCovariance(POSITION_STATE);

  /** output result */
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 0.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 1.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(POSITION_STATE, 2.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 0.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 1.0).getNameValueString() << std::endl;
  std::cout << SimpleGraph.getStateData().getElement(OFFSET_STATE, 2.0).getNameValueString() << std::endl <<std::endl;


  return 0;
}

#endif // TESTMODE