#pragma once

#include "utils.hpp"
#include "coloring.hpp"
//#include <map>;

namespace cap {

template<class T>
class bag {
	/*std::*/
	map<T, size_t> data_;
public:
	typedef typename map<T, size_t>::const_iterator const_iterator;

	void put(const T& t, size_t mult) {
		VERIFY(mult > 0);
		data_[t] += mult;
	}

	void put(const T& t) {
		put(t, 1);
	}

	bool take(const T& t, size_t mult) {
		VERIFY(mult > 0);
		/*typename map<T, size_t>::iterator*/auto it = data_.find(t);
		if (it == data_.end()) {
			return false;
		} else {
			size_t have = it->second;
			if (have < mult) {
				data_.erase(it->first);
				return false;
			} else if (have == mult) {
				data_.erase(it->first);
				return true;
			} else {
				it->second -= mult;
				return true;
			}
		}
	}

	bool take(const T& t) {
		return take(t, 1);
	}

	size_t mult(const T& t) const {
		auto it = data_.find(t);
		if (it == data_.end()) {
			return 0;
		} else {
			return it->second;
		}
	}

	void clear() {
		data_.clear();
	}

	const_iterator begin() const {
		return data_.begin();
	}

	const_iterator end() const {
		return data_.end();
	}

};

template<class Graph>
class GenomePath: public GraphActionHandler<Graph> {
	typedef GraphActionHandler<Graph> base;
public:
	typedef typename Graph::EdgeId EdgeId;
	typedef typename vector<EdgeId>::const_iterator iterator;
private:
	vector<EdgeId> path_;
	bool cyclic_;
	bag<EdgeId> edge_mult_;

	int Idx(size_t idx) {
		if (cyclic_) {
			return idx % path_.size();
		}
		if (idx >= path_.size())
			return -1;
		return idx;
	}

	bool CheckAllMatch(const vector<EdgeId>& edges, size_t pos) {
		for (size_t j = 0; j < edges.size(); ++j) {
			int idx = Idx(pos + j);
			if (idx > 0) {
				if (path_[idx] != edges[j])
					return false;
			} else {
				return false;
			}
		}
		return true;
	}

	bool CheckNoneMatch(const vector<EdgeId>& edges, size_t pos) {
		for (size_t j = 0; j < edges.size(); ++j) {
			int idx = Idx(pos + j);
			if (idx > 0) {
				if (path_[idx] == edges[j])
					return false;
			} else {
				return true;
			}
		}
		return true;
	}

	bool CheckConsistency(const vector<EdgeId>& edges, size_t pos) {
		return CheckAllMatch(edges, pos) || CheckNoneMatch(edges, pos);
//		if (Idx(pos) < 0)
//			return true;
//		bool first_matched = (path_[idx] == edges[0]);
//		for (size_t j = 0; j < edges.size(); ++j) {
//			int idx = Idx(pos + j);
//			if (idx > 0) {
//				if (first_matched ^ (path_[idx] == edges[j]) != 0)
//					return false;
//			} else {
//				return !first_matched;
//			}
//		}
//		return true;
	}

	bool CheckConsistency(const vector<EdgeId>& edges) {
		VERIFY(!edges.empty());
		for (size_t i = 0; i < path_.size(); ++i) {
			if (!CheckConsistency(edges, i)) {
				return false;
			}
		}
		return true;
	}

	void MovePrefixBack(size_t prefix_length) {
		VERIFY(cyclic_);
		vector<EdgeId> tmp(path_.begin(), path_.begin() + prefix_length);
		path_.erase(path_.begin(), path_.begin() + prefix_length);
		path_.insert(path_.end(), tmp.begin(), tmp.end());
	}

	void SubstituteNonBorderFragment(size_t start_pos, size_t end_pos,
			const vector<EdgeId>& subst) {
		VERIFY(start_pos < end_pos && end_pos < path_.size());
		ChangeMult(start_pos, end_pos, subst);
		path_.insert(
				path_.erase(path_.begin() + start_pos, path_.begin() + end_pos),
				subst.begin(), subst.end());
	}

	void FillEdgeMult() {
		for (auto it = path_.begin(); it != path_.end(); ++it) {
			edge_mult_.put(*it);
		}
	}

	void ChangeMult(size_t start_pos, size_t end_pos,
			const vector<EdgeId>& subst) {
		for (size_t i = start_pos; i < end_pos; ++i) {
			bool could_take = edge_mult_.take(path_[i]);
			VERIFY(could_take);
		}
		for (auto it = subst.begin(); it != subst.end(); ++it) {
			edge_mult_.put(*it);
		}
	}

public:
	GenomePath(const Graph& g, const vector<EdgeId>& path, bool cyclic = false) :
			base(g, "GenomePath"), path_(path), cyclic_(cyclic) {
		FillEdgeMult();
	}

	/*virtual*/
	void HandleMerge(const vector<EdgeId>& old_edges, EdgeId new_edge) {
		VERIFY(CheckConsistency(old_edges));
		auto it = find(path_.begin(), path_.end(), old_edges.front());
		while (it != path_.end()) {
			size_t start = it - path_.begin();
			size_t end = start + old_edges.size();
			Substitute(start, end, vector<EdgeId> { new_edge });
		}
	}

	//for cyclic paths, end_pos might be > path.size()
	//might change indices unexpectedly
	void Substitute(size_t start_pos, size_t end_pos,
			const vector<EdgeId>& subst) {
		if (end_pos <= path_.size()) {
			SubstituteNonBorderFragment(start_pos, end_pos, subst);
		} else {
			size_t prefix_length = end_pos - path_.size();
			VERIFY(start_pos >= prefix_length);
			MovePrefixBack(prefix_length);
			SubstituteNonBorderFragment(start_pos - prefix_length, path_.size(),
					subst);
		}
	}

	size_t size() const {
		return path_.size();
	}

	iterator begin() const {
		return path_.begin();
	}

	iterator end() const {
		return path_.end();
	}

	size_t mult(EdgeId e) {
		return edge_mult_.mult(e);
	}

};

template<class Graph>
class AssemblyPathCallback: public PathProcessor<Graph>::Callback {
public:
	typedef typename Graph::EdgeId EdgeId;

private:
	const Graph& g_;
	const ColorHandler<Graph>& coloring_;
	const edge_type assembly_color_;
	size_t edge_count_;

	std::vector<vector<EdgeId>> paths_;

	bool CheckPath(const vector<EdgeId>& path) const {
		if (path.size() > edge_count_)
			return false;
		for (auto it = path.begin(); it != path.end(); ++it) {
			if ((coloring_.Color(*it) & assembly_color_) == 0) {
				return false;
			}
		}
		return true;
	}

public:

	AssemblyPathCallback(const Graph& g, const ColorHandler<Graph>& coloring,
			edge_type assembly_color, size_t edge_count) :
			g_(g), coloring_(coloring), assembly_color_(assembly_color), edge_count_(
					edge_count) {
	}

	virtual void HandlePath(const vector<EdgeId>& path) {
		if (CheckPath(path)) {
			paths_.push_back(path);
		}
	}

	size_t size() const {
		return paths_.size();
	}

	std::vector<vector<EdgeId>> paths() const {
		return paths_;
	}
};

template<class Graph>
class SimpleInDelCorrector {
	typedef typename Graph::VertexId VertexId;
	typedef typename Graph::EdgeId EdgeId;
	Graph& g_;
	ColorHandler<Graph>& coloring_;

	//become invalidated during process
//	const EdgesPositionHandler<Graph>& edge_pos_;
	GenomePath<Graph> genome_path_;
	const edge_type genome_color_;
	const edge_type assembly_color_;

	vector<EdgeId> FindAssemblyPath(VertexId start, VertexId end,
			size_t edge_count_bound, size_t min_length, size_t max_length) {
		AssemblyPathCallback<Graph> assembly_callback(g_, coloring_,
				assembly_color_, edge_count_bound);
		PathProcessor<Graph> path_finder(g_, min_length, max_length, start, end,
				assembly_callback);
		path_finder.Process();
		if (assembly_callback.size() == 1) {
			return assembly_callback.paths().front();
		}
		return {};
	}

	int TryFindGenomePath(size_t pos, VertexId end, size_t edge_count_bound) {
		for (size_t i = 0;
				i + pos < genome_path_.size() && i < edge_count_bound; ++i) {
			if (g_.EdgeEnd(*(genome_path_.begin() + pos + i)) == end) {
				return pos + i + 1;
			}
		}
		return -1;
	}

//	bag<edge_type> ColorLengths(const vector<EdgeId>& edges) {
//		bag<edge_type> answer;
//		for (size_t i = 0; i < edges.size(); ++i) {
//			answer.put(coloring_.Color(edges[i]), g_.length(edges[i]));
//		}
//		return answer;
//	}

	size_t VioletLengthOfGenomeUnique(const vector<EdgeId>& edges) {
		size_t answer = 0;
		for (size_t i = 0; i < edges.size(); ++i) {
			if (coloring_.Color(edges[i]) == edge_type::violet
					&& genome_path_.mult(edges[i]) == 1) {
				answer += g_.length(edges[i]);
			}
		}
		return answer;
	}

	//genome pos exclusive
//	size_t CumulativeGenomeLengthToPos(size_t pos) {
//		size_t answer = 0;
//		for (size_t i = 0; i < pos; ++i) {
//			answer += g_.length(genome_path_[i]);
//		}
//		return answer;
//	}

	bool CheckGenomePath(size_t genome_start, size_t genome_end) {
		return VioletLengthOfGenomeUnique(
				vector<EdgeId>(genome_path_.begin() + genome_start,
						genome_path_.begin() + genome_end)) < 25;
	}

	optional<pair<size_t, size_t>> FindGenomePath(VertexId start, VertexId end,
			size_t edge_count_bound) {
		for (size_t i = 0; i < genome_path_.size(); ++i) {
			if (g_.EdgeStart(*(genome_path_.begin() + i)) == start) {
				int path_end = TryFindGenomePath(i, end, edge_count_bound);
				if (path_end > 0 && CheckGenomePath(i, path_end))
					return make_optional(make_pair(size_t(i), size_t(path_end)));
			}
		}
		return boost::none;
	}

	void RemoveObsoleteEdges(const vector<EdgeId>& edges) {
		for (auto it = edges.begin(); it != edges.end(); ++it) {
			if (coloring_.Color(*it) == genome_color_
					&& genome_path_.mult(*it) == 0) {
				DEBUG("Removing edge " << g_.str(*it) << " as obsolete");
				g_.DeleteEdge(*it);
				g_.CompressVertex(g_.EdgeStart(*it));
				if (!g_.RelatedVertices(g_.EdgeStart(*it), g_.EdgeEnd(*it))) {
					g_.CompressVertex(g_.EdgeEnd(*it));
				}
			}
		}
	}

	string GenomePathStr(size_t genome_start, size_t genome_end) const {
		return g_.str(
				vector<EdgeId>(genome_path_.begin() + genome_start,
						genome_path_.begin() + genome_end));
	}

	void GenPicAlongPath(const vector<EdgeId> path, size_t cnt) {
		make_dir("ref_correction");
		WriteComponentsAlongPath(g_, StrGraphLabeler<Graph>(g_),
				"ref_correction/" + ToString(cnt) + ".dot", 1000, 15,
				TrivialMappingPath(g_, path), *ConstructColorer(coloring_));
	}

	void GenPicAroundEdge(EdgeId e, size_t cnt) {
		make_dir("ref_correction");
		WriteComponentsAroundEdge(g_, e,
				"ref_correction/" + ToString(cnt) + ".dot",
				*ConstructColorer(coloring_), StrGraphLabeler<Graph>(g_));
	}

	void CorrectGenomePath(size_t genome_start, size_t genome_end,
			const vector<EdgeId>& assembly_path) {
		static size_t cnt = 0;
		DEBUG(
				"Case " << ++cnt << " Substituting genome path " << GenomePathStr(genome_start, genome_end) << " with assembly path " << g_.str(assembly_path));
		vector<EdgeId> genomic_edges;
		for (size_t i = genome_start; i < genome_end; ++i) {
			genomic_edges.push_back(*(genome_path_.begin() + i));
		}
		GenPicAlongPath(genomic_edges, cnt * 100);
		GenPicAlongPath(assembly_path, cnt * 100 + 1);
		for (size_t i = 0; i < assembly_path.size(); ++i) {
			coloring_.Paint(assembly_path[i], genome_color_);
		}
		genome_path_.Substitute(genome_start, genome_end, assembly_path);
		RemoveObsoleteEdges(genomic_edges);
		GenPicAroundEdge(*(genome_path_.begin() + genome_start), cnt * 100 + 2);
	}

//	pair<string, pair<size_t, size_t>> ContigIdAndPositions(EdgeId e) {
//		vector<EdgePosition> poss = edge_pos_.GetEdgePositions(e);
//		VERIFY(!poss.empty());
//		if (poss.size() > 1) {
//			WARN("Something strange with assembly positions");
//			return make_pair("", make_pair(0, 0));
//		}
//		EdgePosition pos = poss.front();
//		return make_pair(pos.contigId_, make_pair(pos.start(), pos.end()));
//	}

//	void WriteAltPath(EdgeId e, const vector<EdgeId>& genome_path) {
//		LengthIdGraphLabeler<Graph> basic_labeler(g_);
//		EdgePosGraphLabeler<Graph> pos_labeler(g_, edge_pos_);
//
//		CompositeLabeler<Graph> labeler(basic_labeler, pos_labeler);
//
//		string alt_path_folder = folder_ + ToString(g_.int_id(e)) + "/";
//		make_dir(alt_path_folder);
//		WriteComponentsAlongPath(g_, labeler, alt_path_folder + "path.dot", /*split_length*/
//		1000, /*vertex_number*/15, TrivialMappingPath(g_, genome_path),
//				*ConstructBorderColorer(g_, coloring_));
//	}

//todo use contig constraints here!!!
	void AnalyzeGenomeEdge(EdgeId e) {
		DEBUG("Analysing shortcut genome edge " << g_.str(e));
		DEBUG("Multiplicity " << genome_path_.mult(e));
		if (genome_path_.mult(e) == 1) {
			vector<EdgeId> assembly_path = FindAssemblyPath(g_.EdgeStart(e),
					g_.EdgeEnd(e), 100, 0, g_.length(e) + 1000);
			if (!assembly_path.empty()) {
				DEBUG("Assembly path " << g_.str(assembly_path));
				auto it = std::find(genome_path_.begin(), genome_path_.end(),
						e);
				VERIFY(it != genome_path_.end());
				size_t pos = it - genome_path_.begin();
				CorrectGenomePath(pos, pos + 1, assembly_path);
			} else {
				DEBUG("Couldn't find assembly path");
			}
		}
	}

	void AnalyzeAssemblyEdge(EdgeId e) {
		DEBUG("Analysing shortcut assembly edge " << g_.str(e));
		optional<pair<size_t, size_t>> genome_path = FindGenomePath(
				g_.EdgeStart(e), g_.EdgeEnd(e), /*edge count bound*//*100*/
				300);
		if (genome_path) {
			CorrectGenomePath(genome_path->first, genome_path->second,
					vector<EdgeId> { e });
		} else {
			DEBUG("Empty genome path");
		}
	}

public:
	SimpleInDelCorrector(Graph& g, ColorHandler<Graph>& coloring,
			const vector<EdgeId> genome_path, edge_type genome_color,
			edge_type assembly_color) :
			g_(g), coloring_(coloring), genome_path_(g_, genome_path), genome_color_(
					genome_color), assembly_color_(assembly_color) {
	}

	void Analyze() {
		for (auto it = g_.SmartEdgeBegin(); !it.IsEnd(); ++it) {
			if (coloring_.Color(*it) == genome_color_) {
				AnalyzeGenomeEdge(*it);
			}
			if (coloring_.Color(*it) == assembly_color_) {
				AnalyzeAssemblyEdge(*it);
			}
		}
	}

private:
	DECL_LOGGER("SimpleInDelCorrector")
	;
};

}
