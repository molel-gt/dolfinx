// Copyright (C) 2015-2019 Chris Richardson, Garth N. Wells and Igor Baratta
//
// This file is part of DOLFINx (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#include "IndexMapNew.h"
#include "IndexMap.h"
#include <algorithm>
#include <dolfinx/common/sort.h>
#include <functional>
#include <numeric>

using namespace dolfinx;
using namespace dolfinx::common;

namespace
{
//-----------------------------------------------------------------------------
/// Compute the owning rank of ghost indices
[[maybe_unused]] std::vector<int>
get_ghost_ranks(MPI_Comm comm, std::int32_t local_size,
                const xtl::span<const std::int64_t>& ghosts)
{
  int mpi_size = -1;
  MPI_Comm_size(comm, &mpi_size);
  std::vector<std::int32_t> local_sizes(mpi_size);
  MPI_Allgather(&local_size, 1, MPI_INT32_T, local_sizes.data(), 1, MPI_INT32_T,
                comm);

  // NOTE: We do not use std::partial_sum here as it narrows std::int64_t to
  // std::int32_t.
  // NOTE: Using std::inclusive_scan is possible, but GCC prior to 9.3.0
  // only includes the parallel version of this algorithm, requiring
  // e.g. Intel TBB.
  std::vector<std::int64_t> all_ranges(mpi_size + 1, 0);
  std::transform(all_ranges.cbegin(), std::prev(all_ranges.cend()),
                 local_sizes.cbegin(), std::next(all_ranges.begin()),
                 std::plus<std::int64_t>());

  // Compute rank of ghost owners
  std::vector<int> ghost_ranks(ghosts.size(), -1);
  std::transform(ghosts.cbegin(), ghosts.cend(), ghost_ranks.begin(),
                 [&all_ranges](auto ghost)
                 {
                   auto it = std::upper_bound(all_ranges.cbegin(),
                                              all_ranges.cend(), ghost);
                   return std::distance(all_ranges.cbegin(), it) - 1;
                 });

  return ghost_ranks;
}
//-----------------------------------------------------------------------------
} // namespace

//-----------------------------------------------------------------------------
common::IndexMap common::create_old(const IndexMapNew& map)
{
  std::vector<int> src_ranks = map.owners();
  std::sort(src_ranks.begin(), src_ranks.end());
  src_ranks.erase(std::unique(src_ranks.begin(), src_ranks.end()),
                  src_ranks.end());

  auto dest_ranks
      = dolfinx::MPI::compute_graph_edges_nbx(map.comm(), src_ranks);
  return IndexMap(map.comm(), map.size_local(), dest_ranks, map.ghosts(),
                  map.owners());
}
//-----------------------------------------------------------------------------
common::IndexMapNew common::create_new(const IndexMap& map)
{
  return IndexMapNew(map.comm(), map.size_local(), map.ghosts(), map.owners());
}
//-----------------------------------------------------------------------------
/*
std::vector<int32_t> dolfinx::common::compute_owned_indices(
    const xtl::span<const std::int32_t>& indices, const IndexMapNew& map)
{
  // Split indices span into those owned by this process and those that
  // are ghosts. `ghost_indices` contains the position of the ghost in
  // map.ghosts()
  std::vector<std::int32_t> owned;
  std::vector<std::int32_t> ghost_indices;

  // Get number of owned and ghost indices in indicies list to reserve
  // vectors
  // const int num_owned
  //     = std::count_if(indices.begin(), indices.end(),
  //                     [size_local = map.size_local()](std::int32_t index)
  //                     { return index < size_local; });
  // const int num_ghost = indices.size() - num_owned;
  // owned.reserve(num_owned);
  // ghost_indices.reserve(num_ghost);
  std::copy_if(indices.begin(), indices.end(), std::back_inserter(owned),
               [size = map.size_local()](auto idx) { return idx < size; });

  std::vector<int> src;
  std::for_each(indices.begin(), indices.end(),
                [&ghost_indices, size = map.size_local(),
                 &owners = map.owners()](auto idx)
                {
                  if (idx >= size)
                  {
                    std::int32_t p = idx - size_local;
                    ghost_indices.push_back(p);
                    src.push_back[owners[p]];
                  }
                });

  // Create list of src ranks (remote owners of ghost indices)
  std::sort(src.begin(), src.end());
  src.erase(std::unique(src.begin(), src.end()), src.end());

  // Destination ranks
  std::vector<int> dest
      = dolfinx::MPI::compute_graph_edges_nbx(map.comm(), src);
  std::sort(dest.begin(), dest.end());

  // Owner -> ghost rank
  MPI_Comm comm0;
  MPI_Dist_graph_create_adjacent(map.comm(), src.size(), src.data(),
                                 MPI_UNWEIGHTED, dest.size(), dest.data(),
                                 MPI_UNWEIGHTED, MPI_INFO_NULL, false, &comm0);
  // Ghost -> owner rank
  MPI_Comm comm1;
  MPI_Dist_graph_create_adjacent(map.comm(), dest.size(), dest.data(),
                                 MPI_UNWEIGHTED, src.size(), src.data(),
                                 MPI_UNWEIGHTED, MPI_INFO_NULL, false, &comm0);

  // Create an AdjacencyList whose nodes are the processes in the
  // neighborhood and the links for a given process are the ghosts
  // (global numbering) in `indices` owned by that process.
  MPI_Comm reverse_comm = map.comm(IndexMap::Direction::reverse);
  std::vector<std::int32_t> dest_ranks
      = dolfinx::MPI::neighbors(reverse_comm)[1];

  // std::vector<int> ghost_owner_rank;
  // {
  //   std::vector<int> neighbors = dolfinx::MPI::neighbors(
  //       map.comm(common::IndexMap::Direction::forward))[0];
  //   ghost_owner_rank = map.ghost_owners();
  //   std::transform(ghost_owner_rank.cbegin(), ghost_owner_rank.cend(),
  //                  ghost_owner_rank.begin(),
  //                  [&neighbors](auto r) { return neighbors[r]; });
  // }

  const std::vector<std::int64_t>& ghosts = map.ghosts();
  std::vector<std::int64_t> ghosts_to_send;
  std::vector<std::int32_t> ghosts_per_proc(dest_ranks.size(), 0);

  // Loop through all destination ranks in the neighborhood
  for (std::size_t dest_rank_index = 0; dest_rank_index < dest_ranks.size();
       ++dest_rank_index)
  {
    // Loop through all ghost indices on this rank
    for (std::int32_t ghost_index : ghost_indices)
    {
      // Check if the ghost is owned by the destination rank. If so, add
      // that ghost so it is sent to the correct process.
      if (ghost_owner_rank[ghost_index] == dest_ranks[dest_rank_index])
      {
        ghosts_to_send.push_back(ghosts[ghost_index]);
        ghosts_per_proc[dest_rank_index]++;
      }
    }
  }
  // Create a list of partial sums of the number of ghosts per process
  // and create the AdjacencyList
  std::vector<int> send_disp(dest_ranks.size() + 1, 0);
  std::partial_sum(ghosts_per_proc.begin(), ghosts_per_proc.end(),
                   std::next(send_disp.begin(), 1));
  const graph::AdjacencyList<std::int64_t> data_out(std::move(ghosts_to_send),
                                                    std::move(send_disp));

  // Communicate ghosts on this process in `indices` back to their owners
  const graph::AdjacencyList<std::int64_t> data_in
      = dolfinx::MPI::neighbor_all_to_all(reverse_comm, data_out);

  // Get the local index from the global indices received from other
  // processes and add to `owned`
  const std::vector<std::int64_t>& global_indices = map.global_indices();
  std::vector<std::pair<std::int64_t, std::int32_t>> global_to_local;
  global_to_local.reserve(global_indices.size());
  for (auto idx : global_indices)
  {
    global_to_local.push_back(
        {idx, static_cast<std::int32_t>(global_to_local.size())});
  }
  std::sort(global_to_local.begin(), global_to_local.end());
  std::transform(
      data_in.array().cbegin(), data_in.array().cend(),
      std::back_inserter(owned),
      [&global_to_local](std::int64_t global_index)
      {
        auto it = std::lower_bound(
            global_to_local.begin(), global_to_local.end(),
            typename decltype(global_to_local)::value_type(global_index, 0),
            [](auto& a, auto& b) { return a.first < b.first; });
        assert(it != global_to_local.end() and it->first == global_index);
        return it->second;
      });

  // Sort `owned` and remove non-unique entries (we could have received
  // the same ghost from multiple other processes)
  dolfinx::radix_sort(xtl::span(owned));
  owned.erase(std::unique(owned.begin(), owned.end()), owned.end());

  return owned;
}
*/
//-----------------------------------------------------------------------------
std::tuple<std::int64_t, std::vector<std::int32_t>,
           std::vector<std::vector<std::int64_t>>,
           std::vector<std::vector<int>>>
common::stack_index_maps(
    const std::vector<
        std::pair<std::reference_wrapper<const common::IndexMapNew>, int>>&
        maps)
{
  // Compute process offset for stacked index map
  const std::int64_t process_offset = std::accumulate(
      maps.begin(), maps.end(), std::int64_t(0),
      [](std::int64_t c, auto& map) -> std::int64_t
      { return c + map.first.get().local_range()[0] * map.second; });

  // Get local offset (into new map) for each index map
  std::vector<std::int32_t> local_sizes;
  std::transform(maps.begin(), maps.end(), std::back_inserter(local_sizes),
                 [](auto map)
                 { return map.second * map.first.get().size_local(); });
  std::vector<std::int32_t> local_offset(local_sizes.size() + 1, 0);
  std::partial_sum(local_sizes.begin(), local_sizes.end(),
                   std::next(local_offset.begin()));

  // Build list of src ranks (ranks that own ghosts)
  std::vector<int> src;
  for (auto& map : maps)
  {
    src.insert(src.end(), map.first.get().owners().begin(),
               map.first.get().owners().end());
    std::sort(src.begin(), src.end());
    src.erase(std::unique(src.begin(), src.end()), src.end());
  }

  // Get destination ranks (ranks that ghost my indices), and sort
  std::vector<int> dest = dolfinx::MPI::compute_graph_edges_nbx(
      maps.at(0).first.get().comm(), src);
  std::sort(dest.begin(), dest.end());

  // Create neighbour comms (0: ghost -> owner, 1: (owner -> ghost)
  MPI_Comm comm0, comm1;
  MPI_Dist_graph_create_adjacent(
      maps.at(0).first.get().comm(), dest.size(), dest.data(), MPI_UNWEIGHTED,
      src.size(), src.data(), MPI_UNWEIGHTED, MPI_INFO_NULL, false, &comm0);
  MPI_Dist_graph_create_adjacent(
      maps.at(0).first.get().comm(), src.size(), src.data(), MPI_UNWEIGHTED,
      dest.size(), dest.data(), MPI_UNWEIGHTED, MPI_INFO_NULL, false, &comm1);

  // NOTE: We could perform each MPI call just once rather than per map,
  // but the complexity may not be worthwhile since this function is
  // typically used for 'block' (rather the nested) problems, which is
  // not the most efficient approach anyway.

  std::vector<std::vector<std::int64_t>> ghosts_new(maps.size());
  std::vector<std::vector<int>> ghost_owners_new(maps.size());

  // For each map, send ghost indices to owner and owners send back the
  // new index
  for (std::size_t m = 0; m < maps.size(); ++m)
  {
    const int bs = maps[m].second;
    const common::IndexMapNew& map = maps[m].first.get();
    const std::vector<std::int64_t>& ghosts = map.ghosts();
    const std::vector<int>& owners = map.owners();

    // For each owning rank (on comm), create vector of this rank's
    // ghosts
    std::vector<std::int64_t> send_indices;
    std::vector<std::int32_t> send_sizes;
    std::vector<std::size_t> ghost_buffer_pos;
    {
      std::vector<std::vector<std::int64_t>> ghost_by_rank(src.size());
      std::vector<std::vector<std::size_t>> pos_to_ghost(src.size());
      for (std::size_t i = 0; i < ghosts.size(); ++i)
      {
        auto it = std::lower_bound(src.begin(), src.end(), owners[i]);
        assert(it != src.end() and *it == owners[i]);
        int r = std::distance(src.begin(), it);
        ghost_by_rank[r].push_back(ghosts[i]);
        pos_to_ghost[r].push_back(i);
      }

      // Count number of ghosts per dest
      std::transform(ghost_by_rank.begin(), ghost_by_rank.end(),
                     std::back_insert_iterator(send_sizes),
                     [](auto& g) { return g.size(); });

      // Send buffer and ghost position to send buffer position
      for (auto& g : ghost_by_rank)
        send_indices.insert(send_indices.end(), g.begin(), g.end());
      for (auto& p : pos_to_ghost)
        ghost_buffer_pos.insert(ghost_buffer_pos.end(), p.begin(), p.end());
    }

    // Send how many indices I ghost to each owner, and receive how many
    // of my indices other ranks ghost
    std::vector<std::int32_t> recv_sizes(dest.size(), 0);
    send_sizes.reserve(1);
    recv_sizes.reserve(1);
    MPI_Neighbor_alltoall(send_sizes.data(), 1, MPI_INT32_T, recv_sizes.data(),
                          1, MPI_INT32_T, comm0);

    // Prepare displacement vectors
    std::vector<int> send_disp(src.size() + 1, 0),
        recv_disp(dest.size() + 1, 0);
    std::partial_sum(send_sizes.begin(), send_sizes.end(),
                     std::next(send_disp.begin()));
    std::partial_sum(recv_sizes.begin(), recv_sizes.end(),
                     std::next(recv_disp.begin()));

    // Send ghost indices to owner, and receive indices
    std::vector<std::int64_t> recv_indices(recv_disp.back());
    MPI_Neighbor_alltoallv(send_indices.data(), send_sizes.data(),
                           send_disp.data(), MPI_INT64_T, recv_indices.data(),
                           recv_sizes.data(), recv_disp.data(), MPI_INT64_T,
                           comm0);

    // For each received index (which I should own), compute its new
    // index in the concatenated index map
    std::vector<std::int64_t> ghost_old_to_new;
    ghost_old_to_new.reserve(recv_indices.size());
    std::int64_t offset_old = map.local_range()[0];
    std::int64_t offset_new = local_offset[m] + process_offset;
    for (std::int64_t idx : recv_indices)
    {
      auto idx_local = idx - offset_old;
      assert(idx_local >= 0);
      ghost_old_to_new.push_back(bs * idx_local + offset_new);
    }

    // Send back/receive new indices
    std::vector<std::int64_t> ghosts_new_idx(send_disp.back());
    MPI_Neighbor_alltoallv(ghost_old_to_new.data(), recv_sizes.data(),
                           recv_disp.data(), MPI_INT64_T, ghosts_new_idx.data(),
                           send_sizes.data(), send_disp.data(), MPI_INT64_T,
                           comm1);

    // Unpack new indices and store owner
    std::vector<std::int64_t>& ghost_idx = ghosts_new[m];
    ghost_idx.resize(bs * map.ghosts().size());
    std::vector<int>& owners_new = ghost_owners_new[m];
    owners_new.resize(bs * map.ghosts().size());
    for (std::size_t i = 0; i < send_disp.size() - 1; ++i)
    {
      int rank = src[i];
      for (int j = send_disp[i]; j < send_disp[i + 1]; ++j)
      {
        std::size_t p = ghost_buffer_pos[j];
        for (int k = 0; k < bs; ++k)
        {
          ghost_idx[bs * p + k] = ghosts_new_idx[j] + k;
          owners_new[bs * p + k] = rank;
        }
      }
    }
  }

  // Destroy communicators
  MPI_Comm_free(&comm0);
  MPI_Comm_free(&comm1);

  return {process_offset, std::move(local_offset), std::move(ghosts_new),
          std::move(ghost_owners_new)};
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
IndexMapNew::IndexMapNew(MPI_Comm comm, std::int32_t local_size) : _comm(comm)
{
  // Get global offset (index), using partial exclusive reduction
  std::int64_t offset = 0;
  const std::int64_t local_size_tmp = local_size;
  MPI_Request request_scan;
  MPI_Iexscan(&local_size_tmp, &offset, 1, MPI_INT64_T, MPI_SUM, comm,
              &request_scan);

  // Send local size to sum reduction to get global size
  MPI_Request request;
  MPI_Iallreduce(&local_size_tmp, &_size_global, 1, MPI_INT64_T, MPI_SUM, comm,
                 &request);

  MPI_Wait(&request_scan, MPI_STATUS_IGNORE);
  _local_range = {offset, offset + local_size};

  // Wait for the MPI_Iallreduce to complete
  MPI_Wait(&request, MPI_STATUS_IGNORE);
}
//-----------------------------------------------------------------------------
IndexMapNew::IndexMapNew(MPI_Comm comm, std::int32_t local_size,
                         const xtl::span<const std::int64_t>& ghosts,
                         const xtl::span<const int>& src_ranks)
    : _comm(comm), _ghosts(ghosts.begin(), ghosts.end()),
      _owners(src_ranks.begin(), src_ranks.end())
{
  assert(size_t(ghosts.size()) == src_ranks.size());
  assert(std::equal(src_ranks.begin(), src_ranks.end(),
                    get_ghost_ranks(comm, local_size, _ghosts).begin()));

  // Get global offset (index), using partial exclusive reduction
  std::int64_t offset = 0;
  const std::int64_t local_size_tmp = (std::int64_t)local_size;
  MPI_Request request_scan;
  MPI_Iexscan(&local_size_tmp, &offset, 1, MPI_INT64_T, MPI_SUM, comm,
              &request_scan);

  // Send local size to sum reduction to get global size
  MPI_Request request;
  MPI_Iallreduce(&local_size_tmp, &_size_global, 1, MPI_INT64_T, MPI_SUM, comm,
                 &request);

  // Wait for MPI_Iexscan to complete (get offset)
  MPI_Wait(&request_scan, MPI_STATUS_IGNORE);
  _local_range = {offset, offset + local_size};

  // Wait for the MPI_Iallreduce to complete
  MPI_Wait(&request, MPI_STATUS_IGNORE);
}
//-----------------------------------------------------------------------------
std::array<std::int64_t, 2> IndexMapNew::local_range() const noexcept
{
  return _local_range;
}
//-----------------------------------------------------------------------------
std::int32_t IndexMapNew::num_ghosts() const noexcept { return _ghosts.size(); }
//-----------------------------------------------------------------------------
std::int32_t IndexMapNew::size_local() const noexcept
{
  return _local_range[1] - _local_range[0];
}
//-----------------------------------------------------------------------------
std::int64_t IndexMapNew::size_global() const noexcept { return _size_global; }
//-----------------------------------------------------------------------------
const std::vector<std::int64_t>& IndexMapNew::ghosts() const noexcept
{
  return _ghosts;
}
//-----------------------------------------------------------------------------
void IndexMapNew::local_to_global(const xtl::span<const std::int32_t>& local,
                                  const xtl::span<std::int64_t>& global) const
{
  assert(local.size() <= global.size());
  const std::int32_t local_size = _local_range[1] - _local_range[0];
  std::transform(
      local.cbegin(), local.cend(), global.begin(),
      [local_size, local_range = _local_range[0], &ghosts = _ghosts](auto local)
      {
        if (local < local_size)
          return local_range + local;
        else
        {
          assert((local - local_size) < (int)ghosts.size());
          return ghosts[local - local_size];
        }
      });
}
//-----------------------------------------------------------------------------
void IndexMapNew::global_to_local(const xtl::span<const std::int64_t>& global,
                                  const xtl::span<std::int32_t>& local) const
{
  const std::int32_t local_size = _local_range[1] - _local_range[0];

  std::vector<std::pair<std::int64_t, std::int32_t>> global_local_ghosts(
      _ghosts.size());
  for (std::size_t i = 0; i < _ghosts.size(); ++i)
    global_local_ghosts[i] = {_ghosts[i], i + local_size};
  std::map<std::int64_t, std::int32_t> global_to_local(
      global_local_ghosts.begin(), global_local_ghosts.end());

  std::transform(global.cbegin(), global.cend(), local.begin(),
                 [range = _local_range,
                  &global_to_local](std::int64_t index) -> std::int32_t
                 {
                   if (index >= range[0] and index < range[1])
                     return index - range[0];
                   else
                   {
                     auto it = global_to_local.find(index);
                     return it != global_to_local.end() ? it->second : -1;
                   }
                 });
}
//-----------------------------------------------------------------------------
std::vector<std::int64_t> IndexMapNew::global_indices() const
{
  const std::int32_t local_size = _local_range[1] - _local_range[0];
  const std::int32_t num_ghosts = _ghosts.size();
  const std::int64_t global_offset = _local_range[0];
  std::vector<std::int64_t> global(local_size + num_ghosts);
  std::iota(global.begin(), std::next(global.begin(), local_size),
            global_offset);
  std::copy(_ghosts.cbegin(), _ghosts.cend(),
            std::next(global.begin(), local_size));
  return global;
}
//-----------------------------------------------------------------------------
MPI_Comm IndexMapNew::comm() const { return _comm.comm(); }
//----------------------------------------------------------------------------
/*
std::pair<IndexMapNew, std::vector<std::int32_t>>
IndexMapNew::create_submap(const xtl::span<const std::int32_t>& indices) const
{
  if (!indices.empty() and indices.back() >= this->size_local())
  {
    throw std::runtime_error(
        "Unowned index detected when creating sub-IndexMap");
  }

  int myrank = 0;
  MPI_Comm_rank(_comm.comm(), &myrank);

  // --- Step 1: Compute new offest for this rank and new global size

  std::int64_t local_size = indices.size();
  std::int64_t offset = 0;
  MPI_Request request_offset;
  MPI_Iexscan(&local_size, &offset, 1, MPI_INT64_T, MPI_SUM, _comm.comm(),
              &request_offset);

  std::int64_t size_global = 0;
  MPI_Request request_size;
  MPI_Iallreduce(&local_size, &size_global, 1, MPI_INT64_T, MPI_SUM,
                 _comm.comm(), &request_size);

  // -- Send ghosts in `indices` to owner

  // Build list of src ranks (ranks that own ghosts)
  std::vector<int> src = this->owners();
  std::sort(src.begin(), src.end());
  src.erase(std::unique(src.begin(), src.end()), src.end());

  // Determine destination ranks (ranks that ghost my indices), and sort
  std::vector<int> dest
      = dolfinx::MPI::compute_graph_edges_nbx(this->comm(), src);
  std::sort(dest.begin(), dest.end());

  // Create neighbourhood comm (ghost -> owner)
  MPI_Comm comm0;
  MPI_Dist_graph_create_adjacent(_comm.comm(), dest.size(), dest.data(),
                                 MPI_UNWEIGHTED, src.size(), src.data(),
                                 MPI_UNWEIGHTED, MPI_INFO_NULL, false, &comm0);

  // Pack ghosts in `indices` to send to owner
  std::vector<std::vector<std::int64_t>> send_data(src.size());
  for (std::size_t i = 0; i < _ghosts.size(); ++i)
  {
    auto it = std::lower_bound(src.begin(), src.end(), _owners[i]);
    assert(it != src.end() and *it == _owners[i]);
    int r = std::distance(src.begin(), it);
    send_data[r].push_back(_ghosts[i]);
  }

  // Count number of ghosts per dest
  std::vector<std::int32_t> send_sizes;
  std::transform(send_data.begin(), send_data.end(),
                 std::back_insert_iterator(send_sizes),
                 [](auto& d) { return d.size(); });

  // Build Send buffer and ghost position to send buffer position
  std::vector<std::int64_t> send_indices;
  for (auto& d : send_data)
    send_indices.insert(send_indices.end(), d.begin(), d.end());

  // Send how many indices I ghost to each owner, and receive how many
  // of my indices other ranks ghost
  std::vector<std::int32_t> recv_sizes(dest.size(), 0);
  send_sizes.reserve(1);
  recv_sizes.reserve(1);
  MPI_Neighbor_alltoall(send_sizes.data(), 1, MPI_INT32_T, recv_sizes.data(), 1,
                        MPI_INT32_T, comm0);

  // Prepare displacement vectors
  std::vector<int> send_disp(src.size() + 1, 0), recv_disp(dest.size() + 1, 0);
  std::partial_sum(send_sizes.begin(), send_sizes.end(),
                   std::next(send_disp.begin()));
  std::partial_sum(recv_sizes.begin(), recv_sizes.end(),
                   std::next(recv_disp.begin()));

  // Send ghost indices to owner, and receive indices
  std::vector<std::int64_t> recv_indices(recv_disp.back());
  MPI_Neighbor_alltoallv(send_indices.data(), send_sizes.data(),
                         send_disp.data(), MPI_INT64_T, recv_indices.data(),
                         recv_sizes.data(), recv_disp.data(), MPI_INT64_T,
                         comm0);

  MPI_Comm_free(&comm0);

  // src ranks for each ghost
  std::vector<int> src_ranks;
  for (std::size_t i = 0; i < recv_sizes.size(); ++i)
    src_ranks.insert(src_ranks.end(), recv_sizes[i], src[i]);

  std::vector<std::int32_t> new_to_old_ghost;
  new_to_old_ghost.reserve(recv_indices.size());
  {
    std::vector<std::pair<std::int64_t, std::int32_t>> ghost_to_pos;
    ghost_to_pos.reserve(_ghosts.size());
    for (auto idx : _ghosts)
    {
      ghost_to_pos.push_back(
          {idx, static_cast<std::int32_t>(ghost_to_pos.size())});
    }
    std::sort(ghost_to_pos.begin(), ghost_to_pos.end());

    for (auto idx : recv_indices)
    {
      auto it = std::lower_bound(ghost_to_pos.begin(), ghost_to_pos.end(),
                                 std::pair<std::int64_t, std::int32_t>{idx, 0},
                                 [](auto& a, auto& b)
                                 { return a.first < b.first; });
      assert(it != ghost_to_pos.end() and it->first == idx);
      new_to_old_ghost.push_back(it->second);
    }
  }

  return {IndexMapNew(_comm.comm(), local_size, recv_indices, src_ranks),
          std::move(new_to_old_ghost)};
}
*/
//-----------------------------------------------------------------------------
