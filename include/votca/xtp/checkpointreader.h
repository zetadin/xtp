/*
 * Copyright 2009-2018 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _VOTCA_XTP_CHECKPOINT_READER_H
#define _VOTCA_XTP_CHECKPOINT_READER_H

#include <H5Cpp.h>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include <votca/tools/vec.h>
#include <votca/xtp/eigen.h>

#include <votca/xtp/checkpoint_utils.h>

namespace votca {
namespace xtp {
using namespace checkpoint_utils;

class CheckpointReader {
 public:
  CheckpointReader(const CptLoc& loc) : CheckpointReader(loc, "/"){};

  CheckpointReader(const CptLoc& loc, const std::string path)
      : _loc(loc), _path(path){};

  template <typename T>
  typename std::enable_if<!std::is_fundamental<T>::value>::type operator()(
      T& var, const std::string& name) {
    try {
      ReadData(_loc, var, name);
    } catch (H5::Exception& error) {
      std::stringstream message;

      message << "Could not read " << name << " from " << _loc.getFileName()
              << ":" << _path << std::endl;

      throw std::runtime_error(message.str());
    }
  }

  template <typename T>
  typename std::enable_if<std::is_fundamental<T>::value &&
                          !std::is_same<T, bool>::value>::type
      operator()(T& var, const std::string& name) {
    try {
      ReadScalar(_loc, var, name);
    } catch (H5::Exception& error) {
      std::stringstream message;
      message << "Could not read " << name << " from " << _loc.getFileName()
              << ":" << _path << "/" << std::endl;

      throw std::runtime_error(message.str());
    }
  }

  void operator()(bool& v, const std::string& name) {
    int temp = int(v);
    try {
      ReadScalar(_loc, temp, name);
    } catch (H5::Exception& error) {
      std::stringstream message;
      message << "Could not read " << name << " from " << _loc.getFileName()
              << ":" << _path << std::endl;

      throw std::runtime_error(message.str());
    }
    v = static_cast<bool>(temp);
  }

  void operator()(std::string& var, const std::string& name) {
    try {
      ReadScalar(_loc, var, name);
    } catch (H5::Exception& error) {
      std::stringstream message;
      message << "Could not read " << name << " from " << _loc.getFileName()
              << ":" << _path << std::endl;

      throw std::runtime_error(message.str());
    }
  }

  CheckpointReader openChild(const std::string& childName) {
    try {
      return CheckpointReader(_loc.openGroup(childName),
                              _path + "/" + childName);
    } catch (H5::Exception& e) {
      std::stringstream message;
      message << "Could not open " << _loc.getFileName() << ":/" << _path << "/"
              << childName << std::endl;

      throw std::runtime_error(message.str());
    }
  }

  int getNumDataSets() { return _loc.getNumObjs(); }

 private:
  CptLoc _loc;
  const std::string _path;
  void ReadScalar(const CptLoc& loc, std::string& var,
                  const std::string& name) {
    const H5::DataType* strType = InferDataType<std::string>::get();

    H5::Attribute attr = loc.openAttribute(name);

    H5std_string readbuf("");

    attr.read(*strType, readbuf);

    var = readbuf;
  }

  template <typename T>
  void ReadScalar(const CptLoc& loc, T& value, const std::string& name) {

    H5::Attribute attr = loc.openAttribute(name);
    const H5::DataType* dataType = InferDataType<T>::get();

    attr.read(*dataType, &value);
  }

  template <typename T>
  void ReadData(const CptLoc& loc, Eigen::MatrixBase<T>& matrix,
                const std::string& name) {

    const H5::DataType* dataType = InferDataType<typename T::Scalar>::get();

    H5::DataSet dataset = loc.openDataSet(name);

    H5::DataSpace dp = dataset.getSpace();

    hsize_t dims[2];
    dp.getSimpleExtentDims(dims, NULL);  // ndims is always 2 for us

    hsize_t matRows = dims[0];
    hsize_t matCols = dims[1];

    matrix.derived().resize(matRows, matCols);

    hsize_t matColSize = matrix.derived().outerStride();

    hsize_t fileRows = matCols;

    hsize_t fStride[2] = {1, fileRows};
    hsize_t fCount[2] = {1, 1};
    hsize_t fBlock[2] = {1, fileRows};

    hsize_t mStride[2] = {matColSize, 1};
    hsize_t mCount[2] = {1, 1};
    hsize_t mBlock[2] = {matCols, 1};

    hsize_t mDim[2] = {matCols, matColSize};
    H5::DataSpace mspace(2, mDim);

    for (hsize_t i = 0; i < matRows; i++) {
      hsize_t fStart[2] = {i, 0};
      hsize_t mStart[2] = {0, i};
      dp.selectHyperslab(H5S_SELECT_SET, fCount, fStart, fStride, fBlock);
      mspace.selectHyperslab(H5S_SELECT_SET, mCount, mStart, mStride, mBlock);
      dataset.read(matrix.derived().data(), *dataType, mspace, dp);
    }
  }

  template <typename T>
  typename std::enable_if<std::is_fundamental<T>::value>::type ReadData(
      const CptLoc& loc, std::vector<T>& v, const std::string& name) {

    H5::DataSet dataset = loc.openDataSet(name);
    H5::DataSpace dp = dataset.getSpace();

    const H5::DataType* dataType = InferDataType<T>::get();

    hsize_t dims[2];
    dp.getSimpleExtentDims(dims, NULL);

    v.resize(dims[0]);

    dataset.read(&(v[0]), *dataType);
  }

  void ReadData(const CptLoc& loc, votca::tools::vec& v,
                const std::string& name) {

    // read tools::vec as a vector of three elements
    std::vector<double> data = {0, 0, 0};
    ReadData(loc, data, name);
    v = votca::tools::vec(data[0], data[1], data[2]);
  }

  void ReadData(const CptLoc& loc, std::vector<votca::tools::vec>& v,
                const std::string& name) {

    CptLoc parent = loc.openGroup(name);
    size_t count = parent.getNumObjs();

    v.resize(count);

    size_t c = 0;
    for (auto& vec : v) {
      ReadData(parent, vec, "ind" + std::to_string(c));
      ++c;
    }
  }
};

}  // namespace xtp
}  // namespace votca

#endif  // _VOTCA_XTP_CHECKPOINT_READER_H
