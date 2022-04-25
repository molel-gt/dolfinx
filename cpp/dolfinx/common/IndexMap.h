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
// Forward declaration
class IndexMap;

/// Given a vector of indices (local numbering, owned or ghost) and an
/// index map, this function returns the indices owned by this process,
/// including indices that might have been in the list of indices on
/// another processes.
/// @param[in] indices List of indices
/// @param[in] map The index map
/// @return Vector of indices owned by the process
std::vector<int32_t>
compute_owned_indices(const xtl::span<const std::int32_t>& indices,
                      const IndexMap& map);

/// Compute layout data and ghost indices for a stacked (concatenated)
/// index map, i.e. 'splice' multiple maps into one. Communication is
/// required to compute the new ghost indices.
///
/// @param[in] maps List of (index map, block size) pairs
/// @returns The (0) global offset of a stacked map for this rank, (1)
/// local offset for each submap in the stacked map, and (2) new indices
/// for the ghosts for each submap (3) owner rank of each ghost entry
/// for each submap
std::tuple<std::int64_t, std::vector<std::int32_t>,
           std::vector<std::vector<std::int64_t>>,
           std::vector<std::vector<int>>>
stack_index_maps(
    const std::vector<
        std::pair<std::reference_wrapper<const common::IndexMap>, int>>& maps);

/// This class represents the distribution index arrays across
/// processes. An index array is a contiguous collection of N+1 indices
/// [0, 1, . . ., N] that are distributed across M processes. On a given
/// process, the IndexMap stores a portion of the index set using local
/// indices [0, 1, . . . , n], and a map from the local indices to a
/// unique global index.

class IndexMap
{
public:
  /// Edge directions of neighborhood communicator
  enum class Direction
  {
    reverse, // Ghost to owner
    forward, // Owner to ghost
  };

  /// Create an non-overlapping index map with local_size owned on this
  /// process.
  ///
  /// @note Collective
  /// @param[in] comm The MPI communicator
  /// @param[in] local_size Local size of the IndexMap, i.e. the number
  /// of owned entries
  IndexMap(MPI_Comm comm, std::int32_t local_size);

  /// Create an index map with local_size owned indiced on this process
  ///
  /// @note Collective
  /// @param[in] comm The MPI communicator
  /// @param[in] local_size Local size of the IndexMap, i.e. the number
  /// of owned entries
  /// @param[in] dest_ranks Ranks that 'ghost' indices that are owned by
  /// the calling rank. I.e., ranks that the caller will send data to
  /// when updating ghost values.
  /// @param[in] ghosts The global indices of ghost entries
  /// @param[in] src_ranks Owner rank (on global communicator) of each
  /// entry in @p ghosts
  IndexMap(MPI_Comm comm, std::int32_t local_size,
           const xtl::span<const int>& dest_ranks,
           const xtl::span<const std::int64_t>& ghosts,
           const xtl::span<const int>& src_ranks);

private:
  template <typename U, typename V, typename W, typename X>
  IndexMap(std::array<std::int64_t, 2> local_range, std::size_t size_global,
           MPI_Comm comm, U&& comm_owner_to_ghost, U&& comm_ghost_to_owner,
           V&& displs_recv_fwd, V&& ghost_pos_recv_fwd, W&& ghosts,
           X&& shared_indices)
      : _local_range(local_range), _size_global(size_global), _comm(comm),
        _comm_owner_to_ghost(std::forward<U>(comm_owner_to_ghost)),
        _comm_ghost_to_owner(std::forward<U>(comm_ghost_to_owner)),
        _ghost_pos_recv_fwd(std::forward<V>(ghost_pos_recv_fwd)),
        _ghosts(std::forward<W>(ghosts)),
        _shared_indices(std::forward<X>(shared_indices))
  {
  }

public:
  // Copy constructor
  IndexMap(const IndexMap& map) = delete;

  /// Move constructor
  IndexMap(IndexMap&& map) = default;

  /// Destructor
  ~IndexMap() = default;

  /// Move assignment
  IndexMap& operator=(IndexMap&& map) = default;

  // Copy assignment
  IndexMap& operator=(const IndexMap& map) = delete;

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

  /// Return a MPI communicator with attached distributed graph topology
  /// information
  /// @param[in] dir Edge direction of communicator (forward, reverse)
  /// @return A neighborhood communicator for the specified edge direction
  MPI_Comm comm(Direction dir) const;

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

  /// Local (owned) indices shared with neighbor processes, i.e. are
  /// ghosts on other processes, grouped by sharing (neighbor) process
  /// (destination ranks in forward communicator and source ranks in the
  /// reverse communicator). `scatter_fwd_indices().links(p)` gives the
  /// list of owned indices that needs to be sent to neighbourhood rank
  /// `p` during a forward scatter.
  ///
  /// Entries are ordered such that `scatter_fwd_indices.offsets()` is
  /// the send displacement array for a forward scatter and
  /// `scatter_fwd_indices.array()[i]` in the index of the owned index
  /// that should be placed at position `i` in the send buffer for a
  /// forward scatter.
  /// @return List of indices that are ghosted on other processes
  const graph::AdjacencyList<std::int32_t>&
  scatter_fwd_indices() const noexcept;

  /// Position of ghost entries in the receive buffer after a forward
  /// scatter, e.g. for a receive buffer `b` and a set operation, the
  /// ghost values should be updated  by `ghost_value[i] =
  /// b[scatter_fwd_ghost_positions[i]]`.
  /// @return Position of the ith ghost entry in the received buffer
  const std::vector<std::int32_t>& scatter_fwd_ghost_positions() const noexcept;

  /// Owner rank on the global communicator of each ghost entry
  std::vector<int> ghost_owner_rank() const;

  /// Compute the owner on the neighborhood communicator of ghost indices
  const std::vector<int>& ghost_owner_neighbor_rank() const;

  /// @todo Aim to remove this function? If it's kept, should it work
  /// with neighborhood ranks?
  ///
  /// Compute map from each local (owned) index to the set of ranks that
  /// have the index as a ghost
  /// @return shared indices
  std::map<std::int32_t, std::set<int>> compute_shared_indices() const;

  /// Create new index map from a subset of indices in this index map.
  /// The order of the indices is preserved, with new map effectively a
  /// 'compressed' map.
  /// @param[in] indices Local indices in the map that should appear in
  /// the new index map. All indices must be owned, i.e. indices must be
  /// less than `this->size_local()`.
  /// @pre `indices` must be sorted and contain no duplicates
  /// @return The (i) new index map and (ii) a map from the ghost
  /// position in the new map to the ghost position in the original
  /// (this) map
  std::pair<IndexMap, std::vector<std::int32_t>>
  create_submap(const xtl::span<const std::int32_t>& indices) const;

private:
  // Range of indices (global) owned by this process
  std::array<std::int64_t, 2> _local_range;

  // Number indices across communicator
  std::int64_t _size_global;

  // MPI communicator (duplicated of 'input' communicator)
  dolfinx::MPI::Comm _comm;

  // Communicator where the source ranks own the indices in the callers
  // halo, and the destination ranks 'ghost' indices owned by the
  // caller. I.e.,
  // - in-edges (src) are from ranks that own my ghosts
  // - out-edges (dest) go to ranks that 'ghost' my owned indices
  dolfinx::MPI::Comm _comm_owner_to_ghost;

  // Communicator where the source ranks have ghost indices that are
  // owned by the caller, and the destination ranks are the owners of
  // indices in the callers halo region. I.e.,
  // - in-edges (src) are from ranks that 'ghost' my owned indicies
  // - out-edges (dest) are to the owning ranks of my ghost indices
  dolfinx::MPI::Comm _comm_ghost_to_owner;

  // Position in the recv buffer for a forward scatter for the ith ghost
  // index (_ghost[i]) entry
  std::vector<std::int32_t> _ghost_pos_recv_fwd;

  // Local-to-global map for ghost indices
  std::vector<std::int64_t> _ghosts;

  // Local-to-global map for ghost indices
  std::vector<int> _ghost_owner_neighbor_rank;

  // List of owned local indices that are in the ghost (halo) region on
  // other ranks, grouped by rank in the neighbor communicator
  // (destination ranks in forward communicator and source ranks in the
  // reverse communicator), i.e. `_shared_indices.num_nodes() ==
  // size(_comm_owner_to_ghost)`. The array _shared_indices.offsets() is
  // equivalent to 'displs_send_fwd'.
  mutable std::unique_ptr<graph::AdjacencyList<std::int32_t>> _shared_indices;
};

} // namespace dolfinx::common
