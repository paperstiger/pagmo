/*****************************************************************************
 *   Copyright (C) 2004-2009 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program; if not, write to the                           *
 *   Free Software Foundation, Inc.,                                         *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.               *
 *****************************************************************************/

#include <boost/numeric/conversion/cast.hpp>
#include <boost/random/uniform_int.hpp>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <utility>

#include "../exceptions.h"
#include "../rng.h"
#include "barabasi_albert.h"
#include "base.h"

namespace pagmo { namespace topology {

/// Constructor from kernel size and number of edges.
/**
 * Build a BA network in which the initial kernel has size m0 and the elements being inserted after the construction of the kernel
 * is completed are connected randomly to m nodes. Will fail if m0 < 2, if m < 1 or if m > m0.
 *
 * @param[in] m0 size of the kernel
 * @param[in] m number of random connections to be established when a new node is added.
 */
barabasi_albert::barabasi_albert(int m0, int m):
	m_m0(boost::numeric_cast<std::size_t>(m0)),m_m(boost::numeric_cast<std::size_t>(m)),
	m_drng(rng_generator::get<rng_double>()),m_urng(rng_generator::get<rng_uint32>())
{
	if (m0 < 2 || m < 1 || m > m0) {
		pagmo_throw(value_error,"the value of m and m0 must be at least 1 and 2, and m must not be greater than m0");
	}
}

/// Clone method.
base_ptr barabasi_albert::clone() const
{
	return base_ptr(new barabasi_albert(*this));
}

/// Connect method.
void barabasi_albert::connect(int n)
{
	pagmo_assert(get_number_of_vertices() > 0);
	const vertices_size_type prev_size = get_number_of_vertices() - 1;
	// Get the iterator of the newly-added index.
	const v_iterator it_n = get_it(n);
	if (prev_size < m_m0) {
		// If we had not built the initial m0 nodes, do it.
		// We want to connect the newcomer island with high probability, and make sure that
		// at least one connection exists (otherwise the island stays isolated).
		// NOTE: is it worth to make it a user-tunable parameter?
		const double prob = 0.8;
		// Flag indicating if at least 1 connection was added.
		bool connection_added = false;
		// Main loop.
		for (std::pair<v_iterator,v_iterator> vertices = get_vertices_it(); vertices.first != vertices.second; ++vertices.first) {
			// Do not consider the new vertex itself.
			if (vertices.first != it_n) {
				if (m_drng() < prob) {
					connection_added = true;
					// Add the connections
					add_edge(vertices.first,it_n);
					add_edge(it_n,vertices.first);
				}
			}
		}
		// If no connections were established and this is not the first island being inserted,
		// establish at least one connection with a random island other than n.
		if ((!connection_added) && (prev_size != 0)) {
			// Get an iterator to the first vertex and advance it by a random quantity between
			// 0 and n_vertices - 1. Repeat the procedure if by chance we end up on it_n again.
			boost::uniform_int<vertices_size_type> uni_int(0,get_number_of_vertices() - 1);
			v_iterator random_it;
			do {
				random_it = get_vertices_it().first;
				std::advance(random_it,uni_int(m_urng));
			} while (random_it == it_n);
			// Add connections to the random vertex.
			add_edge(random_it,it_n);
			add_edge(it_n,random_it);
		}
	} else {
		// Let's find the current total number of edges.
		const edges_size_type n_edges = get_number_of_edges();
		pagmo_assert(n_edges > 0);
		// Now we need to add m edges, choosing the nodes with a probability
		// proportional to their number of connections. We keep track of the
		// connection established in order to avoid connecting twice to the same
		// node.
		boost::uniform_int<edges_size_type> uni_int(0,n_edges - 1);
		std::size_t i = 0;
		while (i < m_m) {
			// Here we choose a random number between 0 and n_edges - 1.
			const edges_size_type rn = uni_int(m_urng);
			edges_size_type n = 0;
			// Iterate over all vertices and accumulate the number of edges for each of them. Stop when the accumulated number of edges is greater
			// than rn. This is equivalent to giving a chance of connection to vertex v directly proportional to the number of edges departing from v.
			// You can think of this process as selecting a random edge among all the existing edges and connecting to the vertex from which the
			// selected edge departs.
			std::pair<v_iterator,v_iterator> vertices = get_vertices_it();
			for (; vertices.first != vertices.second; ++vertices.first) {
				// Do not consider it_n.
				if (vertices.first != it_n) {
					const std::pair<a_iterator,a_iterator> tmp = get_adjacent_vertices(vertices.first);
					n += boost::numeric_cast<edges_size_type>(std::distance(tmp.first,tmp.second));
					if (n > rn) {
						break;
					}
				}
			}
			pagmo_assert(vertices.first != vertices.second);
			// If the candidate was not already connected, then add it.
			if (!are_adjacent(it_n,vertices.first)) {
				add_edge(vertices.first,it_n);
				add_edge(it_n,vertices.first);
				++i;
			}
		}
	}
}

/// Topology-specific human readable info.
/**
 * Will return a formatted string containing the size of the kernel and the number of connections for newly-inserted nodes.
 *
 * @return string containing the parameters of the BA model.
 */
std::string barabasi_albert::human_readable_extra() const
{
	std::ostringstream oss;
	oss << "m0:\t" << m_m0 << '\n';
	oss << "m:\t" << m_m << '\n';
	return oss.str();
}

}}