// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "UnstructuredField.h"
#include "FieldUtils.h"
#include "VolumeImageLoader.h"
// cycles
#include "scene/shader_nodes.h"
#include "util/tbb.h"
#ifdef WITH_CYCLESPHI
#include "scene/image_vdb.h" // RAWImageLoader
#endif
// std
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace anari_cycles {

namespace {

// VTK cell type codes used by KHR_SPATIAL_FIELD_UNSTRUCTURED
constexpr uint8_t CELL_TETRA = 10;
constexpr uint8_t CELL_HEXAHEDRON = 12;
constexpr uint8_t CELL_WEDGE = 13;
constexpr uint8_t CELL_PYRAMID = 14;

int cellVertexCount(uint8_t type)
{
  switch (type) {
  case CELL_TETRA:
    return 4;
  case CELL_HEXAHEDRON:
    return 8;
  case CELL_WEDGE:
    return 6;
  case CELL_PYRAMID:
    return 5;
  default:
    return 0;
  }
}

struct Vec3
{
  double x, y, z;
};

inline Vec3 operator-(const Vec3 &a, const Vec3 &b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Solve [c0 c1 c2] * x = b (columns c0..c2) by Cramer's rule; false when the
// matrix is (nearly) singular.
bool solve3(const Vec3 &c0, const Vec3 &c1, const Vec3 &c2, const Vec3 &b, Vec3 &x)
{
  auto det = [](const Vec3 &a, const Vec3 &b, const Vec3 &c) {
    return a.x * (b.y * c.z - b.z * c.y) - b.x * (a.y * c.z - a.z * c.y)
        + c.x * (a.y * b.z - a.z * b.y);
  };
  const double d = det(c0, c1, c2);
  const double scale = std::abs(c0.x) + std::abs(c0.y) + std::abs(c0.z)
      + std::abs(c1.x) + std::abs(c1.y) + std::abs(c1.z) + std::abs(c2.x)
      + std::abs(c2.y) + std::abs(c2.z);
  if (!(std::abs(d) > 1e-12 * scale * scale * scale))
    return false;
  x = {det(b, c1, c2) / d, det(c0, b, c2) / d, det(c0, c1, b) / d};
  return true;
}

// Barycentric weights of 'p' in tetrahedron (a, b, c, d); true when inside
// (with a small relative tolerance so shared faces are not missed).
bool tetWeights(const Vec3 &p,
    const Vec3 &a,
    const Vec3 &b,
    const Vec3 &c,
    const Vec3 &d,
    double w[4])
{
  Vec3 x;
  if (!solve3(b - a, c - a, d - a, p - a, x))
    return false;
  constexpr double eps = 1e-6;
  w[0] = 1.0 - x.x - x.y - x.z;
  w[1] = x.x;
  w[2] = x.y;
  w[3] = x.z;
  return w[0] >= -eps && w[1] >= -eps && w[2] >= -eps && w[3] >= -eps;
}

// VTK isoparametric shape functions N and their parametric derivatives dN
// for hexahedra, wedges and pyramids.
void shapeFunctions(
    uint8_t type, double r, double s, double t, double N[8], double dN[8][3])
{
  const double rm = 1.0 - r, sm = 1.0 - s, tm = 1.0 - t;
  if (type == CELL_WEDGE) {
    const double u = 1.0 - r - s;
    const double n[6] = {u * tm, r * tm, s * tm, u * t, r * t, s * t};
    const double d[6][3] = {{-tm, -tm, -u},
        {tm, 0.0, -r},
        {0.0, tm, -s},
        {-t, -t, u},
        {t, 0.0, r},
        {0.0, t, s}};
    for (int i = 0; i < 6; i++) {
      N[i] = n[i];
      for (int k = 0; k < 3; k++)
        dN[i][k] = d[i][k];
    }
    return;
  }

  // Bilinear base quad (shared by hexahedra and pyramids)
  const double q[4] = {rm * sm, r * sm, r * s, rm * s};
  const double dq[4][2] = {{-sm, -rm}, {sm, -r}, {s, r}, {-s, rm}};

  if (type == CELL_PYRAMID) {
    for (int i = 0; i < 4; i++) {
      N[i] = q[i] * tm;
      dN[i][0] = dq[i][0] * tm;
      dN[i][1] = dq[i][1] * tm;
      dN[i][2] = -q[i];
    }
    N[4] = t;
    dN[4][0] = 0.0;
    dN[4][1] = 0.0;
    dN[4][2] = 1.0;
    return;
  }

  // hexahedron
  for (int i = 0; i < 4; i++) {
    N[i] = q[i] * tm;
    dN[i][0] = dq[i][0] * tm;
    dN[i][1] = dq[i][1] * tm;
    dN[i][2] = -q[i];
    N[i + 4] = q[i] * t;
    dN[i + 4][0] = dq[i][0] * t;
    dN[i + 4][1] = dq[i][1] * t;
    dN[i + 4][2] = q[i];
  }
}

// Interpolation weights of 'p' in the cell with vertices 'v' (VTK ordering);
// returns false when 'p' lies outside the cell.
bool cellWeights(uint8_t type, const Vec3 *v, const Vec3 &p, double w[8])
{
  const int nv = cellVertexCount(type);
  for (int i = 0; i < 8; i++)
    w[i] = 0.0;

  if (type == CELL_TETRA)
    return tetWeights(p, v[0], v[1], v[2], v[3], w);

  // Newton iteration on the isoparametric mapping x(r,s,t) = sum N_i v_i
  double rst[3] = {0.5, 0.5, 0.5};
  if (type == CELL_WEDGE)
    rst[0] = rst[1] = 1.0 / 3.0;
  else if (type == CELL_PYRAMID)
    rst[2] = 0.2;

  bool converged = false;
  double N[8], dN[8][3];
  for (int iter = 0; iter < 20; iter++) {
    shapeFunctions(type, rst[0], rst[1], rst[2], N, dN);
    Vec3 x{0.0, 0.0, 0.0}, c[3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (int i = 0; i < nv; i++) {
      x.x += N[i] * v[i].x;
      x.y += N[i] * v[i].y;
      x.z += N[i] * v[i].z;
      for (int k = 0; k < 3; k++) {
        c[k].x += dN[i][k] * v[i].x;
        c[k].y += dN[i][k] * v[i].y;
        c[k].z += dN[i][k] * v[i].z;
      }
    }
    Vec3 delta;
    if (!solve3(c[0], c[1], c[2], p - x, delta))
      break;
    rst[0] += delta.x;
    rst[1] += delta.y;
    rst[2] += delta.z;
    if (!std::isfinite(rst[0]) || !std::isfinite(rst[1])
        || !std::isfinite(rst[2]))
      break;
    if (std::max({std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)})
        < 1e-7) {
      converged = true;
      break;
    }
  }

  if (converged) {
    constexpr double eps = 1e-5;
    const double r = rst[0], s = rst[1], t = rst[2];
    bool inside = t >= -eps && t <= 1.0 + eps && r >= -eps && s >= -eps;
    if (type == CELL_WEDGE)
      inside = inside && r + s <= 1.0 + eps;
    else
      inside = inside && r <= 1.0 + eps && s <= 1.0 + eps;
    if (!inside)
      return false;
    shapeFunctions(type,
        std::clamp(r, 0.0, 1.0),
        std::clamp(s, 0.0, 1.0),
        std::clamp(t, 0.0, 1.0),
        N,
        dN);
    for (int i = 0; i < nv; i++)
      w[i] = N[i];
    return true;
  }

  // Newton failed (degenerate or strongly distorted cell, or the pyramid
  // apex): fall back to a tetrahedral decomposition.
  static const int hexTets[6][4] = {{0, 1, 2, 6},
      {0, 2, 3, 6},
      {0, 3, 7, 6},
      {0, 7, 4, 6},
      {0, 4, 5, 6},
      {0, 5, 1, 6}};
  static const int wedgeTets[3][4] = {{0, 1, 2, 3}, {1, 2, 3, 4}, {2, 3, 4, 5}};
  static const int pyramidTets[2][4] = {{0, 1, 2, 4}, {0, 2, 3, 4}};
  const int(*tets)[4] = hexTets;
  int numTets = 6;
  if (type == CELL_WEDGE) {
    tets = wedgeTets;
    numTets = 3;
  } else if (type == CELL_PYRAMID) {
    tets = pyramidTets;
    numTets = 2;
  }
  for (int k = 0; k < numTets; k++) {
    const int *tv = tets[k];
    double tw[4];
    if (tetWeights(p, v[tv[0]], v[tv[1]], v[tv[2]], v[tv[3]], tw)) {
      for (int i = 0; i < 4; i++)
        w[tv[i]] = tw[i];
      return true;
    }
  }
  return false;
}

// Read element 'i' of an index array (uint32 or uint64)
inline uint64_t readIndex(const Array1D *array, size_t i)
{
  if (array->elementType() == ANARI_UINT64)
    return array->beginAs<uint64_t>()[i];
  return array->beginAs<uint32_t>()[i];
}

bool isIndexType(anari::DataType type)
{
  return type == ANARI_UINT32 || type == ANARI_UINT64;
}

// Dense float grid image sampled through the voxel attribute path: the
// CyclesPhi raw 3D texture when available, a Cycles VDB grid otherwise.
// Voxel (i,j,k) sits at origin + (i,j,k) * spacing.
ccl::ImageHandle addDenseGridImage(CyclesGlobalState &state,
    const std::vector<float> &voxels,
    const anari_vec::uint3 &dims,
    const anari_vec::float3 &origin,
    const anari_vec::float3 &spacing,
    const char *name)
{
#ifdef WITH_CYCLESPHI
  ccl::vector<char> bytes(voxels.size() * sizeof(float));
  std::memcpy(bytes.data(), voxels.data(), bytes.size());
  return addFieldImage(state,
      std::make_unique<ccl::RAWImageLoader>(bytes,
          ccl::make_int3(dims[0], dims[1], dims[2]),
          ccl::make_float3(spacing[0], spacing[1], spacing[2]),
          ccl::make_float3(origin[0], origin[1], origin[2]),
          ccl::make_int3(0, 0, 0),
          ccl::make_int3(dims[0] - 1, dims[1] - 1, dims[2] - 1),
          ccl::RAWImageLoader::eRawFloat,
          1),
      INTERPOLATION_LINEAR);
#elif defined(ANARI_CYCLES_HAS_VDB)
  openvdb::initialize();
  const ccl::Transform objectToTexture =
      transform_scale(make_float3(1.f / (dims[0] * spacing[0]),
          1.f / (dims[1] * spacing[1]),
          1.f / (dims[2] * spacing[2])))
      * transform_translate(make_float3(-origin[0], -origin[1], -origin[2]));
  return addFieldImage(state,
      std::make_unique<FieldVDBImageLoader>(
          voxels.data(), dims, objectToTexture, name),
      INTERPOLATION_LINEAR);
#else
  (void)state;
  (void)voxels;
  (void)dims;
  (void)origin;
  (void)spacing;
  (void)name;
  return ccl::ImageHandle();
#endif
}

} // namespace

UnstructuredField::UnstructuredField(CyclesGlobalState *s)
    : SpatialField(s),
      m_vertexPosition(this),
      m_vertexData(this),
      m_index(this),
      m_cellData(this),
      m_cellType(this),
      m_cellIndex(this)
{}

UnstructuredField::~UnstructuredField() = default;

void UnstructuredField::commitParameters()
{
  m_vertexPosition = getParamObject<Array1D>("vertex.position");
  m_vertexData = getParamObject<Array1D>("vertex.data");
  m_index = getParamObject<Array1D>("index");
  m_cellData = getParamObject<Array1D>("cell.data");
  m_cellType = getParamObject<Array1D>("cell.type");
  m_cellIndex = getParamObject<Array1D>("cell.index");
  m_resolution = getParam<int>("cyclesphi.resolution", 0);
  if (const char *env = getenv("CYCLES_ANARI_UNSTRUCTURED_RESOLUTION"))
    m_resolution = atoi(env);
}

void UnstructuredField::finalize()
{
  m_voxelImage = ccl::ImageHandle();
  m_coverageImage = ccl::ImageHandle();
  m_values.clear();
  m_valid = resample();
  SpatialField::finalize();
}

bool UnstructuredField::resample()
{
  auto warn = [&](const char *msg) {
    reportMessage(ANARI_SEVERITY_WARNING, "'unstructured' field: %s", msg);
    return false;
  };

  if (!m_vertexPosition || !m_index || !m_cellType)
    return warn(
        "missing required parameter 'vertex.position', 'index' or "
        "'cell.type'");
  if (m_vertexPosition->elementType() != ANARI_FLOAT32_VEC3)
    return warn("'vertex.position' must be an array of FLOAT32_VEC3");
  if (!isIndexType(m_index->elementType()))
    return warn("'index' must be an array of UINT32 or UINT64");
  if (m_cellType->elementType() != ANARI_UINT8)
    return warn("'cell.type' must be an array of UINT8");
  if (m_cellIndex && !isIndexType(m_cellIndex->elementType()))
    return warn("'cell.index' must be an array of UINT32 or UINT64");

  const bool vertexCentric = bool(m_vertexData);
  const Array1D *data = vertexCentric ? m_vertexData.get() : m_cellData.get();
  if (!data)
    return warn("neither 'vertex.data' nor 'cell.data' is set");
  if (!voxelToFloatSupported(data->elementType()))
    return warn("unsupported 'vertex.data'/'cell.data' element type");

  const size_t numVerts = m_vertexPosition->size();
  const size_t numIndices = m_index->size();
  const size_t numCells = m_cellType->size();
  if (numVerts == 0 || numCells == 0)
    return warn("no vertices or cells");
  if (vertexCentric && data->size() < numVerts)
    return warn("'vertex.data' has fewer entries than 'vertex.position'");
  if (!vertexCentric && data->size() < numCells)
    return warn("'cell.data' has fewer entries than 'cell.type'");

  std::vector<float> values(data->size());
  convertVoxelsToFloat(
      data->elementType(), data->begin(), 0, values.data(), values.size());

  const auto *positions = m_vertexPosition->beginAs<anari_vec::float3>();
  const uint8_t *types = m_cellType->beginAs<uint8_t>();

  // First index of every cell: 'cell.index' or, when absent, the running sum
  // of the cell vertex counts.
  std::vector<uint64_t> cellBegin(numCells, 0);
  {
    uint64_t running = 0;
    for (size_t c = 0; c < numCells; c++) {
      cellBegin[c] = (m_cellIndex && c < m_cellIndex->size())
          ? readIndex(m_cellIndex.get(), c)
          : running;
      running = cellBegin[c] + cellVertexCount(types[c]);
    }
  }

  // Validate cells and compute the bounds of the referenced vertices.
  std::vector<uint8_t> cellOk(numCells, 0);
  box3 b = empty_box3();
  size_t numValidCells = 0, numSkipped = 0;
  for (size_t c = 0; c < numCells; c++) {
    const int nv = cellVertexCount(types[c]);
    if (nv == 0 || cellBegin[c] + nv > numIndices) {
      numSkipped++;
      continue;
    }
    bool ok = true;
    for (int k = 0; k < nv && ok; k++)
      ok = readIndex(m_index.get(), cellBegin[c] + k) < numVerts;
    if (!ok) {
      numSkipped++;
      continue;
    }
    for (int k = 0; k < nv; k++) {
      const auto &p = positions[readIndex(m_index.get(), cellBegin[c] + k)];
      extend(b, make_float3(p[0], p[1], p[2]));
    }
    cellOk[c] = 1;
    numValidCells++;
  }
  if (numSkipped > 0) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'unstructured' field: skipped %zu cells with an unknown type or "
        "out-of-range indices",
        numSkipped);
  }
  if (numValidCells == 0)
    return warn("no valid cells");

  // Grid resolution: the voxel budget grows with the cell count (between
  // 128^3 and 256^3 voxels) unless 'cyclesphi.resolution' fixes the voxel
  // count along the longest axis.
  const float3 extent = b.upper - b.lower;
  const float maxExtent = std::max({extent.x, extent.y, extent.z});
  if (!(maxExtent > 0.f) || !std::isfinite(maxExtent))
    return warn("degenerate cell bounds");
  float h;
  if (m_resolution > 1) {
    h = maxExtent / float(std::min(m_resolution, 2048) - 1);
  } else {
    const double budget = std::clamp(
        double(numValidCells) * 512.0, 128.0 * 128 * 128, 256.0 * 256 * 256);
    const double volume = double(std::max(extent.x, maxExtent * 1e-3f))
        * std::max(extent.y, maxExtent * 1e-3f)
        * std::max(extent.z, maxExtent * 1e-3f);
    h = float(std::cbrt(volume / budget));
    h = std::max(h, maxExtent / 1023.f);
  }
  for (int a = 0; a < 3; a++) {
    const float e = extent[a];
    const uint32_t n = std::max(2u, uint32_t(std::ceil(e / h)) + 1u);
    m_dims[a] = n;
    m_origin[a] = b.lower[a];
    m_spacing[a] = e > 0.f ? e / float(n - 1) : h;
  }

  const size_t nx = m_dims[0], ny = m_dims[1], nz = m_dims[2];
  const size_t numVoxels = nx * ny * nz;
  constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
  m_values.assign(numVoxels, NaN);

  // Rasterize the cells, parallel over Z bands (each band only writes its own
  // slices, so cells overlapping several bands never race).
  const size_t bandSlices = std::max<size_t>(1, nz / 64);
  const size_t numBands = (nz + bandSlices - 1) / bandSlices;
  std::vector<std::vector<uint32_t>> bandCells(numBands);
  auto voxelRange = [&](float lo, float hi, int a, size_t n) {
    const float i0 = std::ceil((lo - m_origin[a]) / m_spacing[a] - 1e-4f);
    const float i1 = std::floor((hi - m_origin[a]) / m_spacing[a] + 1e-4f);
    const long long first = std::max(0ll, (long long)i0);
    const long long last = std::min((long long)n - 1, (long long)i1);
    return std::make_pair(first, last);
  };

  std::vector<std::array<Vec3, 8>> cellVerts(numCells);
  std::vector<std::array<uint32_t, 8>> cellVertIds(numCells);
  for (size_t c = 0; c < numCells; c++) {
    if (!cellOk[c])
      continue;
    const int nv = cellVertexCount(types[c]);
    float zlo = std::numeric_limits<float>::max(), zhi = -zlo;
    for (int k = 0; k < nv; k++) {
      const auto id = uint32_t(readIndex(m_index.get(), cellBegin[c] + k));
      const auto &p = positions[id];
      cellVertIds[c][k] = id;
      cellVerts[c][k] = {p[0], p[1], p[2]};
      zlo = std::min(zlo, p[2]);
      zhi = std::max(zhi, p[2]);
    }
    const auto [z0, z1] = voxelRange(zlo, zhi, 2, nz);
    if (z0 > z1)
      continue;
    for (size_t band = size_t(z0) / bandSlices; band <= size_t(z1) / bandSlices;
         band++)
      bandCells[band].push_back(uint32_t(c));
  }

  // Value of cell 'c' at 'p'; false when 'p' lies outside the cell.
  auto evalCell = [&](uint32_t c, const Vec3 &p, float &value) {
    const uint8_t type = types[c];
    double w[8];
    if (!cellWeights(type, cellVerts[c].data(), p, w))
      return false;
    if (vertexCentric) {
      double sum = 0.0;
      for (int n = 0; n < cellVertexCount(type); n++)
        sum += w[n] * values[cellVertIds[c][n]];
      value = float(sum);
    } else {
      value = values[c];
    }
    return true;
  };
  auto voxelPosition = [&](double i, double j, double k) {
    return Vec3{double(m_origin[0]) + i * m_spacing[0],
        double(m_origin[1]) + j * m_spacing[1],
        double(m_origin[2]) + k * m_spacing[2]};
  };

  // Pass 1: classify the voxel centers (containing cell + value), parallel
  // over Z bands (each band only writes its own slices, so cells overlapping
  // several bands never race).
  std::vector<int32_t> cellId(numVoxels, -1);
  ccl::parallel_for(size_t(0), numBands, [&](size_t band) {
    const long long bandZ0 = (long long)(band * bandSlices);
    const long long bandZ1 =
        std::min((long long)nz - 1, (long long)((band + 1) * bandSlices) - 1);
    for (const uint32_t c : bandCells[band]) {
      const int nv = cellVertexCount(types[c]);
      const Vec3 *v = cellVerts[c].data();
      Vec3 lo = v[0], hi = v[0];
      for (int k = 1; k < nv; k++) {
        lo = {std::min(lo.x, v[k].x),
            std::min(lo.y, v[k].y),
            std::min(lo.z, v[k].z)};
        hi = {std::max(hi.x, v[k].x),
            std::max(hi.y, v[k].y),
            std::max(hi.z, v[k].z)};
      }
      const auto [x0, x1] = voxelRange(float(lo.x), float(hi.x), 0, nx);
      const auto [y0, y1] = voxelRange(float(lo.y), float(hi.y), 1, ny);
      auto [z0, z1] = voxelRange(float(lo.z), float(hi.z), 2, nz);
      z0 = std::max(z0, bandZ0);
      z1 = std::min(z1, bandZ1);
      for (long long k = z0; k <= z1; k++)
        for (long long j = y0; j <= y1; j++)
          for (long long i = x0; i <= x1; i++) {
            const size_t idx = (size_t(k) * ny + size_t(j)) * nx + size_t(i);
            if (cellId[idx] >= 0)
              continue; // already covered by a neighboring cell
            float value;
            if (evalCell(c, voxelPosition(double(i), double(j), double(k)), value)) {
              cellId[idx] = int32_t(c);
              m_values[idx] = value;
            }
          }
    }
  });

  // Pass 2: coverage and value * coverage grids for the volume path. Voxels
  // whose center classification differs from a face neighbor straddle the
  // mesh boundary: their coverage is the fraction of S^3 sub-samples over
  // the voxel inside a cell (tested against the cells found around it), so
  // the interpolated coverage crosses 1/2 at the true boundary instead of
  // following a voxel staircase.
  constexpr int S = 4;
  std::vector<float> coverage(numVoxels, 0.f), weighted(numVoxels, 0.f);
  ccl::parallel_for(size_t(0), nz, [&](size_t k) {
    std::vector<uint32_t> candidates;
    for (size_t j = 0; j < ny; j++)
      for (size_t i = 0; i < nx; i++) {
        const size_t idx = (k * ny + j) * nx + i;
        const bool inside = cellId[idx] >= 0;
        auto insideAt = [&](long long ii, long long jj, long long kk) {
          if (ii < 0 || jj < 0 || kk < 0 || ii >= (long long)nx
              || jj >= (long long)ny || kk >= (long long)nz)
            return false;
          return cellId[(size_t(kk) * ny + size_t(jj)) * nx + size_t(ii)] >= 0;
        };
        const long long I = i, J = j, K = k;
        const bool boundary = insideAt(I - 1, J, K) != inside
            || insideAt(I + 1, J, K) != inside || insideAt(I, J - 1, K) != inside
            || insideAt(I, J + 1, K) != inside || insideAt(I, J, K - 1) != inside
            || insideAt(I, J, K + 1) != inside;
        if (!boundary) {
          coverage[idx] = inside ? 1.f : 0.f;
          weighted[idx] = inside ? m_values[idx] : 0.f;
          continue;
        }

        candidates.clear();
        if (inside)
          candidates.push_back(uint32_t(cellId[idx]));
        for (long long dk = -1; dk <= 1; dk++)
          for (long long dj = -1; dj <= 1; dj++)
            for (long long di = -1; di <= 1; di++) {
              const long long ii = I + di, jj = J + dj, kk = K + dk;
              if (ii < 0 || jj < 0 || kk < 0 || ii >= (long long)nx
                  || jj >= (long long)ny || kk >= (long long)nz)
                continue;
              const int32_t c =
                  cellId[(size_t(kk) * ny + size_t(jj)) * nx + size_t(ii)];
              if (c >= 0
                  && std::find(candidates.begin(), candidates.end(), uint32_t(c))
                      == candidates.end())
                candidates.push_back(uint32_t(c));
            }

        int hits = 0;
        double sum = 0.0;
        for (int sk = 0; sk < S; sk++)
          for (int sj = 0; sj < S; sj++)
            for (int si = 0; si < S; si++) {
              const Vec3 p = voxelPosition(double(i) + (si + 0.5) / S - 0.5,
                  double(j) + (sj + 0.5) / S - 0.5,
                  double(k) + (sk + 0.5) / S - 0.5);
              for (const uint32_t c : candidates) {
                float value;
                if (evalCell(c, p, value)) {
                  hits++;
                  sum += value;
                  break;
                }
              }
            }
        coverage[idx] = float(hits) / float(S * S * S);
        weighted[idx] = hits > 0 ? float(sum / hits) * coverage[idx] : 0.f;
      }
  });

  size_t covered = 0;
  for (size_t i = 0; i < numVoxels; i++)
    covered += cellId[i] >= 0;

  if (covered == 0) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'unstructured' field: no grid sample (%zu x %zu x %zu) lies inside "
        "a cell; increase 'cyclesphi.resolution'",
        nx,
        ny,
        nz);
  }

  reportMessage(ANARI_SEVERITY_DEBUG,
      "'unstructured' field: resampled %zu cells onto a %zu x %zu x %zu grid "
      "(%zu voxels covered)",
      numValidCells,
      nx,
      ny,
      nz,
      covered);

  auto &state = *deviceState();
  m_voxelImage = addDenseGridImage(state,
      weighted,
      m_dims,
      m_origin,
      m_spacing,
      "ANARI unstructured value");
  if (m_voxelImage.empty())
    return warn("this build has no dense voxel grid support (needs CyclesPhi "
                "or OpenVDB/NanoVDB)");
  m_coverageImage = addDenseGridImage(state,
      coverage,
      m_dims,
      m_origin,
      m_spacing,
      "ANARI unstructured coverage");
  if (m_coverageImage.empty()) {
    m_voxelImage = ccl::ImageHandle();
    return warn("could not create the coverage grid");
  }
  return true;
}

bool UnstructuredField::isValid() const
{
  return m_valid;
}

ccl::ShaderOutput *UnstructuredField::createCyclesSamplingNodes(
    ccl::ShaderGraph *graph)
{
  if (!isValid())
    return nullptr;
  auto *weighted = createVoxelSamplingNodes(graph);
  auto *coverage = createVoxelCoverageNodes(graph);
  if (!weighted || !coverage)
    return nullptr;

  // value = (value * coverage) / coverage, guarded where coverage vanishes
  auto *guard = graph->create_node<ccl::MathNode>();
  guard->set_math_type(ccl::NODE_MATH_MAXIMUM);
  graph->connect(coverage, guard->input("Value1"));
  guard->set_value2(1e-6f);

  auto *value = graph->create_node<ccl::MathNode>();
  value->set_math_type(ccl::NODE_MATH_DIVIDE);
  graph->connect(weighted, value->input("Value1"));
  graph->connect(guard->output("Value"), value->input("Value2"));
  return value->output("Value");
}

ccl::ShaderOutput *UnstructuredField::createCyclesCoverageNodes(
    ccl::ShaderGraph *graph)
{
  return isValid() ? createVoxelCoverageNodes(graph) : nullptr;
}

box3 UnstructuredField::bounds() const
{
  if (!isValid())
    return empty_box3();
  box3 b;
  for (int a = 0; a < 3; a++) {
    b.lower[a] = m_origin[a];
    b.upper[a] = m_origin[a] + (m_dims[a] - 1.f) * m_spacing[a];
  }
  return b;
}

float UnstructuredField::stepSize() const
{
  return std::min({std::abs(m_spacing[0]),
      std::abs(m_spacing[1]),
      std::abs(m_spacing[2])});
}

bool UnstructuredField::getDenseVoxelGrid(std::vector<float> &voxels,
    anari_vec::uint3 &dims,
    anari_vec::float3 &origin,
    anari_vec::float3 &spacing) const
{
  // Only valid once finalized; the isosurface consumer is re-finalized after
  // this field (change observation) and converges on the resampled grid.
  if (!isValid() || m_values.empty())
    return false;
  voxels = m_values;
  dims = m_dims;
  origin = m_origin;
  spacing = m_spacing;
  return true;
}

} // namespace anari_cycles
