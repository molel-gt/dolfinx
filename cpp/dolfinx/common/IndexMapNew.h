// Copyright (C) 2015-2019 Chris Richardson, Garth N. Wells and Igor Baratta
//
// This file is part of DOLFINx (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <dolfinx/common/MPI.h>
#include <dolfinx/graph/AdjacencyList.h>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>
#include <xtl/xspan.hpp>

namespace dolfinx::common
{
class IndexMapNew;
class IndexMap;

/// @brief List of owned indices that are ghosted by another process
IndexMap create_old(const IndexMapNew& map);

// /// @brief List of owned indices that are ghosted by another process
// std::vector<std::int32_t> halo_owned_indices(const IndexMapNew& map);

/// This class represents the distribution index arrays across
/// processes. An index array is a contiguous collection of N+1 indices
/// [0, 1, . . ., N] that are distributed across M processes. On a given
/// process, the IndexMap stores a portion of the index set using local
/// indices [0, 1, . . . , n], and a map from the local indices to a
/// unique global index.
class IndexMapNew
{
public:
  /// Create an non-overlapping index map with local_size owned on this
  /// process.
  ///
  /// @note Collective
  /// @param[in] comm The MPI communicator
  /// @param[in] local_size Local size of the IndexMap, i.e. the number
  /// of owned entries
  IndexMapNew(MPI_Comm comm, std::int32_t local_size);

  /// Create an index map with local_size owned indiced on this process
  ///
  /// @note Collective
  /// @param[in] comm The MPI communicator
  /// @param[in] local_size Local size of the IndexMap, i.e. the number
  /// of owned entries
  /// @param[in] ghosts The global indices of ghost entries
  /// @param[in] src_ranks Owner rank (on global communicator) of each
  /// entry in @p ghosts
  IndexMapNew(MPI_Comm comm, std::int32_t local_size,
              const xtl::span<const std::int64_t>& ghosts,
              const xtl::span<const int>& src_ranks);

  // private:
  //   template <typename U, typename V, typename W, typename X>
  //   IndexMap(std::array<std::int64_t, 2> local_range, std::size_t
  //   size_global,
  //            MPI_Comm comm, U&& comm_owner_to_ghost, U&& comm_ghost_to_owner,
  //            V&& displs_recv_fwd, V&& ghost_pos_recv_fwd, W&& ghosts,
  //            X&& shared_indices)
  //       : _local_range(local_range), _size_global(size_global), _comm(comm),
  //         _comm_owner_to_ghost(std::forward<U>(comm_owner_to_ghost)),
  //         _comm_ghost_to_owner(std::forward<U>(comm_ghost_to_owner)),
  //         _displs_recv_fwd(std::forward<V>(displs_recv_fwd)),
  //         _ghost_pos_recv_fwd(std::forward<V>(ghost_pos_recv_fwd)),
  //         _ghosts(std::forward<W>(ghosts)),
  //         _shared_indices(std::forward<X>(shared_indices))
  //   {
  //     _sizes_recv_fwd.resize(_displs_recv_fwd.size() - 1, 0);
  //     std::adjacent_difference(_displs_recv_fwd.cbegin() + 1,
  //                              _displs_recv_fwd.cend(),
  //                              _sizes_recv_fwd.begin());

  //     const std::vector<int32_t>& displs_send = _shared_indices->offsets();
  //     _sizes_send_fwd.resize(_shared_indices->num_nodes(), 0);
  //     std::adjacent_difference(displs_send.cbegin() + 1, displs_send.cend(),
  //                              _sizes_send_fwd.begin());
  //   }

public:
  // Copy constructor
  IndexMapNew(const IndexMapNew& map) = delete;

  /// Move constructor
  IndexMapNew(IndexMapNew&& map) = default;

  /// Destructor
  ~IndexMapNew() = default;

  /// Move assignment
  IndexMapNew& operator=(IndexMapNew&& map) = default;

  // Copy assignment
  IndexMapNew& operator=(const IndexMapNew& map) = delete;

  /// Range of indices (global) owned by this process
  std::array<std::int64_t, 2> local_range() const noexcept;

  /// Number of ghost indices on this process
  std::int32_t num_ghosts() const noexcept;

  /// Number of indices owned by on this process
  std::int32_t size_local() const noexcept;

  /// Number indices across communicator
  std::int64_t size_global() const noexcept;

  /// Local-to-global map for ghosts (local indexing beyond end of local
  /// range)
  const std::vector<std::int64_t>& ghosts() const noexcept;

  /// Return the MPI communicator used to create the index map
  /// @return Communicator
  MPI_Comm comm() const;

  /// Compute global indices for array of local indices
  /// @param[in] local Local indices
  /// @param[out] global The global indices
  void local_to_global(const xtl::span<const std::int32_t>& local,
                       const xtl::span<std::int64_t>& global) const;

  /// Compute local indices for array of global indices
  /// @param[in] global Global indices
  /// @param[out] local The local of the corresponding global index in 'global'.
  /// Returns -1 if the local index does not exist on this process.
  void global_to_local(const xtl::span<const std::int64_t>& global,
                       const xtl::span<std::int32_t>& local) const;

  /// Global indices
  /// @return The global index for all local indices (0, 1, 2, ...) on
  /// this process, including ghosts
  std::vector<std::int64_t> global_indices() const;

  /// @brief Ghost owner
  const std::vector<int>& ghost_owners() const;

private:
  // Range of indices (global) owned by this process
  std::array<std::int64_t, 2> _local_range;

  // Number indices across communicator
  std::int64_t _size_global;

  // MPI communicator (duplicated of 'input' communicator)
  dolfinx::MPI::Comm _comm;

  // Local-to-global map for ghost indices
  std::vector<std::int64_t> _ghosts;

  // Owning rank
  std::vector<int> _ghost_owners;
};

} // namespace dolfinx::common