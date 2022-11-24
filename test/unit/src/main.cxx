#include <boost/ut.hpp>

#include <autob/graph.hxx>

using namespace boost::ut;

int main()
{
	"DependencyGraph"_test = [] {
		autob::DependencyGraph<autob::Identifiable> graph;
		graph.add({"a"});
		graph.add({"b"});
		graph.add({"c"});

		should("have correct size") = [&] { expect(graph.size() == 3_ul); };

		graph.depend({"a"}, {"b"});
		graph.depend({"b"}, {"c"});

		// c -> b -> a
		should("sort topologically") = [&] {
			auto sorted_opt = graph.sorted();
			expect(sorted_opt.has_value());
			auto const& sorted = *sorted_opt;
			expect(sorted == std::vector<autob::Identifiable>{{"c"}, {"b"}, {"a"}});
		};
	};
}