/*
 * This file is part of Panopticon (http://panopticon.re).
 * Copyright (C) 2014 Kai Michaelis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <list>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/variant.hpp>

#include <panopticon/ensure.hh>

#pragma once

namespace po
{
	template<typename T>
	struct tree
	{
		class iterator : public boost::iterator_facade<
			iterator,
			T&,
			boost::forward_traversal_tag,
			T&>
		{
		public:
			iterator() : _adaptee(), _items(nullptr) {};
			explicit iterator(std::list<int>::iterator a, std::unordered_map<int, T> *i, boost::variant<std::shared_ptr<std::list<int>>, const std::list<int>*> l)
				: _list(l), _adaptee(a), _items(i) {};

			iterator &increment(void) { ++_adaptee; return *this; };
			iterator &decrement(void) { --_adaptee; return *this; };

			T& dereference(void) const { return _items->at(*_adaptee); }
			bool equal(const iterator &a) const
			{
				struct isend_visitor : public boost::static_visitor < bool >
				{
					isend_visitor(std::list<int>::const_iterator i) : _iter(i) {}
					bool operator()(std::shared_ptr<std::list<int>> s) const { return !s || this->_iter == s->end(); }
					bool operator()(const std::list<int>* s) const { return !s || this->_iter == s->end(); }
					std::list<int>::const_iterator _iter;
				};

				bool e1 = boost::apply_visitor(isend_visitor(_adaptee), _list);
				bool e2 = boost::apply_visitor(isend_visitor(a._adaptee), a._list);

				return (e1 && e2) || (!e1 && !e2 && *_adaptee == *a._adaptee);
			}

			void advance(size_t sz) { std::advance(_adaptee, sz); }

		private:
			boost::variant<std::shared_ptr<std::list<int>>, const std::list<int>*> _list;
			std::list<int>::iterator _adaptee;
			std::unordered_map<int, T> *_items;

			friend struct tree<T>;
		};

		class const_iterator : public boost::iterator_facade<
			const_iterator,
			const T&,
			boost::forward_traversal_tag,
			const T&>
		{
		public:
			const_iterator() : _adaptee(), _items(nullptr) {};
			const_iterator(iterator i) : _list(i._list), _adaptee(i._adaptee), _items(i._items) {}
			explicit const_iterator(std::list<int>::const_iterator a, const std::unordered_map<int, T> *i, boost::variant<std::shared_ptr<std::list<int>>, const std::list<int>*> l)
				: _list(l), _adaptee(a), _items(i) {};

			const_iterator &increment(void) { ++_adaptee; return *this; };
			const_iterator &decrement(void) { --_adaptee; return *this; };

			const T& dereference(void) const { return _items->at(*_adaptee); }
			bool equal(const const_iterator &a) const
			{
				struct isend_visitor : public boost::static_visitor < bool >
				{
					isend_visitor(std::list<int>::const_iterator i) : _iter(i) {}
					bool operator()(std::shared_ptr<std::list<int>> s) const { return !s || this->_iter == s->end(); }
					bool operator()(const std::list<int>* s) const { return !s || this->_iter == s->end(); }
					std::list<int>::const_iterator _iter;
				};

				bool e1 = boost::apply_visitor(isend_visitor(_adaptee), _list);
				bool e2 = boost::apply_visitor(isend_visitor(a._adaptee), a._list);

				return (e1 && e2) || (!e1 && !e2 && *_adaptee == *a._adaptee);
			}

			void advance(size_t sz) { std::advance(_adaptee, sz); }

		private:
			boost::variant<std::shared_ptr<std::list<int>>, const std::list<int>*> _list;
			std::list<int>::const_iterator _adaptee;
			const std::unordered_map<int, T> *_items;

			friend struct tree<T>;
		};


		tree(void) : tree(std::move(T())) {}
		tree(const T& t) : tree(std::move(T(t))) {}
		tree(T&& t) : _next(1), _items(), _parents(), _children()
		{
			auto i = _next++;
			_items.emplace(i, t);
			_parents.emplace(i, 0);
			_children.emplace(i, std::list<int>());
			_children.emplace(0, std::list<int>({ i }));
		}
		tree(const tree& t) : _next(t._next.load()), _items(t._items), _parents(t._parents), _children(t._children) {}
		tree(tree&& t) : _next(t._next.load()), _items(std::move(t._items)), _parents(std::move(t._parents)), _children(std::move(t._children)) {}

		tree& operator=(const tree& t)
		{
			_next.store(t._next.load());
			_items = t._items;
			_parents = t._parents;
			_children = t._children;

			return *this;
		}

		tree& operator=(tree&& t)
		{
			_next.store(t._next.load());
			_items = std::move(t._items);
			_parents = std::move(t._parents);
			_children = std::move(t._children);

			return *this;
		}

		bool operator==(const tree& t) const
		{
			auto a = depth_first_search(croot(), *this);
			auto b = depth_first_search(t.croot(), t);

			while (a.first != a.second)
				if (b.first == b.second || *a.first++ != *b.first++)
					return false;
			return a.first == a.second && b.first == b.second;
		}

		bool operator!=(const tree& t) const { return !operator==(t); }

		iterator root(void) { return iterator(_children.at(0).begin(), &_items, &_children.at(0)); }
		const_iterator root(void) const { return croot(); }
		const_iterator croot(void) const { return const_iterator(_children.at(0).begin(), &_items, &_children.at(0)); }

		iterator begin(iterator i) { return iterator(_children.at(*i._adaptee).begin(), &_items, &_children.at(*i._adaptee)); }
		const_iterator begin(const_iterator i) const { return cbegin(i); }
		const_iterator cbegin(const_iterator i) const { return const_iterator(_children.at(*i._adaptee).cbegin(), &_items, &_children.at(*i._adaptee)); }

		iterator end(iterator i) { return iterator(_children.at(*i._adaptee).end(), &_items, &_children.at(*i._adaptee)); }
		const_iterator end(const_iterator i) const { return cend(i); }
		const_iterator cend(const_iterator i) const { return const_iterator(_children.at(*i._adaptee).cend(), &_items, &_children.at(*i._adaptee)); }

		template<typename M>
		static tree<T> from_map(const M& m)
		{
			// look for root
			for (auto p : m)
			{
				if (p.first == p.second)
				{
					tree<T> ret(p.first);
					std::function<void(tree<T>::iterator)> fn;

					fn = [&](iterator i)
					{
						for (auto q : m)
						{
							if (q.second == *i && q.first != q.second)
							{
								auto j = ret.insert(i, q.first);
								fn(j);
							}
						}
					};
					fn(ret.root());
					return ret;
				}
			}

			throw std::runtime_error("no root");
		}

		static std::pair<const_iterator, const_iterator> depth_first_search(const_iterator i, const tree &t)
		{
			std::shared_ptr<std::list<int>> tmp = std::make_shared<std::list<int>>();
			std::unordered_set<int> visited;
			std::function<void(int)> fn;

			fn = [&](int i) -> void
			{
				for (int c : t._children.at(i))
					if (!visited.count(c))
						fn(c);

				tmp->emplace_back(i);
				visited.emplace(i);
			};

			fn(*i._adaptee);

			ensure(tmp->size() == visited.size() && visited.size() == t._items.size());
			return std::make_pair(const_iterator(tmp->begin(), &t._items, tmp), const_iterator(tmp->end(), &t._items, tmp));
		}

		iterator insert(iterator p, const T& itm)
		{
			auto i = _next++;
			_items.emplace(i, itm);
			_parents.emplace(i, *p._adaptee);
			_children[*p._adaptee].emplace_back(i);
			_children.emplace(std::make_pair(i, std::list<int>()));

			return iterator(std::prev(_children[*p._adaptee].end()), &_items, &_children.at(*p._adaptee));
		}

		void remove(iterator i)
		{
			int idx = *i._adaptee;

			while (_children.at(idx).size())
				remove(iterator(_children.at(idx).begin(), &_items, &_children.at(idx)));

			if (idx > 1)
			{
				_children.at(_parents.at(idx)).remove(idx);
				_children.erase(_children.find(idx));
				_items.erase(idx);
			}
			else
			{
				_items[idx] = T();
			}
		}

		static std::string graphviz(const tree<T>& t)
		{
			std::stringstream ss;

			ss << "digraph G {" << std::endl;
			for (auto p : t._parents)
				ss << "n_" << p.second << " -> n_" << p.first << std::endl;
			ss << "}";

			return ss.str();
		}

		size_t size(void) const
		{
			return _items.size();
		}

	private:
		std::atomic<int> _next;
		std::unordered_map<int, T> _items;
		std::unordered_map<int, int> _parents;
		std::unordered_map<int, std::list<int>> _children;
	};
}
