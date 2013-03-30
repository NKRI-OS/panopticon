#include <algorithm>

#include <flowgraph.hh>
#include <basic_block.hh>

using namespace po;
using namespace std;

odotstream &po::operator<<(odotstream &os, const flowgraph &f)
{
	os << "digraph G" << endl
		 << "{" << endl
		 << "\tgraph [label=\"" << f.name << "\"];" << endl;

	for(proc_cptr p: f.procedures)
	{
		os << *p << endl;

		if(os.calls)
			for(proc_cwptr q: p->callees)
				os << "\t" 
					 << (os.body ? "cluster_" : "") << unique_name(*p) 
					 << " -> " 
					 << (os.body ? "cluster_" : "") << unique_name(*q.lock())
					 << ";" << endl;
	}

	os << "}" << endl;
	return os;
}

string po::unique_name(const flowgraph &f)
{
	return f.name.empty() ? "flow_" + to_string((uintptr_t)&f) : f.name;
}

proc_ptr po::find_procedure(flow_ptr fg, addr_t a)
{
	std::set<proc_ptr>::iterator i = fg->procedures.begin();
	
	while(i != fg->procedures.end())
		if(find_bblock(*i,a))
			return *i;
		else 
			++i;
	
	return proc_ptr(0);
}

bool po::has_procedure(flow_ptr flow, addr_t entry)
{
	return any_of(flow->procedures.begin(),flow->procedures.end(),[&](const proc_ptr p) 
								{ return p->entry->area().includes(entry); });
}
