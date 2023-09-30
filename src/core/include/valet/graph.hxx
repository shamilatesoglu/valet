#pragma once

// external
#include <spdlog/spdlog.h>

// stl
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <stack>

namespace valet
{

struct Identifiable {
	std::string id;
	bool operator==(Identifiable const& rhs) const { return id == rhs.id; }
	operator std::string() const { return id; }
};

} // namespace valet

namespace std
{
template <>
struct hash<valet::Identifiable> {
	size_t operator()(const valet::Identifiable& key) const
	{
		return ::std::hash<std::string>()(key.id);
	}
};
} // namespace std

namespace valet
{

template <typename T>
class DependencyGraph
{
public:
	bool add(T const& node)
	{
		if (depgraph.contains(node))
			return false;
		spdlog::trace("DependencyGraph: Adding node {}", std::string(node));
		depgraph[node] = {};
		return true;
	}

	bool depend(T const& dependant, T const& dependence)
	{
		auto it = depgraph.find(dependant);
		if (it == depgraph.end()) {
			spdlog::error("Cannot find {} in dependency graph", std::string(dependant));
			return false;
		}
		it->second.insert(dependence);
		return true;
	}

	bool is_valid() const
	{
		// TODO: Check cycle
		return true;
	}

	std::optional<std::vector<T>> sorted() const
	{
		// TODO: Implement a better way to check for cycles
		class TopologicalSorter
		{
		public:
			TopologicalSorter(DependencyGraph<T> const& graph)
			    : graph(graph), visited(graph.size()), on_stack(graph.size())
			{
			}

			// Returns false if a cycle is detected
			bool sort()
			{
				for (auto const& [node, _] : graph) {
					if (!visited.contains(node)) {
						if (!dfs(node)) {
							return false;
						}
					}
				}
				return true;
			}

			std::vector<T> sorted;

		private:
			bool dfs(T const& cur)
			{
				visited.insert(cur);
				on_stack.insert(cur);

				for (auto const& dep : graph.immediate_deps(cur)) {
					if (on_stack.contains(dep)) {
						// Cycle detected
						spdlog::error(
						    "Cycle detected in dependency graph! {} -> {}",
						    std::string(cur), std::string(dep));
						return false;
					}
					if (!visited.contains(dep)) {
						if (!dfs(dep)) {
							return false;
						}
					}
				}

				sorted.push_back(cur);
				on_stack.erase(cur);
				return true;
			}

			DependencyGraph<T> const& graph;
			std::unordered_set<T> visited;
			std::unordered_set<T> on_stack;
		};

		TopologicalSorter sorter(*this);
		if (!sorter.sort())
			return std::nullopt;

		return sorter.sorted;
	}

	std::unordered_set<T> const& immediate_deps(T const& node) const
	{
		auto const& it = depgraph.find(node);
		if (it == depgraph.end()) {
			spdlog::error("Node {} not found in dependency graph!", std::string(node));
			std::exit(1);
		}
		return it->second;
	}

	std::vector<T> all_deps(T const& node) const
	{
		std::vector<T> all_deps;
		std::stack<T> stack;
		for (auto const& dep : immediate_deps(node)) {
			stack.push(dep);
		}
		while (!stack.empty()) {
			auto cur = stack.top();
			all_deps.push_back(cur);
			stack.pop();
			for (auto const& dep : immediate_deps(cur)) {
				stack.push(dep);
			}
		}
		return all_deps;
	}

	std::unordered_set<T> immediate_dependants(T const& dependence) const
	{
		std::unordered_set<T> dependants;
		for (auto const& [pkg, deps] : depgraph)
			for (auto const& dep : deps)
				if (dep == dependence)
					dependants.insert(pkg);
		return dependants;
	}

	std::unordered_set<T> all_dependants(T const& dependence) const
	{
		// Find the dependants of this dependence
		std::stack<T> stack;
		std::unordered_set<T> dependants;
		for (auto const& dep : immediate_dependants(dependence)) {
			stack.push(dep);
		}
		while (!stack.empty()) {
			auto cur = stack.top();
			stack.pop();
			dependants.insert(cur);
			for (auto const& dep : immediate_dependants(cur)) {
				stack.push(dep);
			}
		}
		return dependants;
	}

	bool empty() const { return depgraph.empty(); }

	T const* get_node_by_id(Identifiable const& identifiable) const
	{
		spdlog::trace("Searching for {}", std::string(identifiable));
		auto it = depgraph.find(static_cast<T const&>(identifiable));
		if (it != depgraph.end()) {
			return &it->first;
		}
		return nullptr;
	}

	bool has(Identifiable const& identifiable) const
	{
		return depgraph.find(static_cast<T const&>(identifiable)) != depgraph.end();
	}

	typename std::unordered_map<T, std::unordered_set<T>>::const_iterator begin() const
	{
		return depgraph.begin();
	}

	typename std::unordered_map<T, std::unordered_set<T>>::const_iterator end() const
	{
		return depgraph.end();
	}

	size_t size() const { return depgraph.size(); }

private:
	std::unordered_map<T, std::unordered_set<T>> depgraph;
};

} // namespace valet
