#ifndef _TG_NODES_HXX
#define _TG_NODES_HXX

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdlib>

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/algorithm.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/Search_traits_adapter.h>     /* Just use two dimensional lookup - elevation is a trait */
#include <boost/iterator/zip_iterator.hpp>

#include <simgear/compiler.h>
#include <simgear/bucket/newbucket.hxx>
#include <simgear/io/lowlevel.hxx>

#include "tg_unique_tgnode.hxx"

#define FG_PROXIMITY_EPSILON 0.000001
#define FG_COURSE_EPSILON 0.0001


typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_2 Point;
typedef boost::tuple<Point, double, TGNode*> Point_and_Elevation;

//definition of the property map
#ifdef CGAL_OLD
struct My_point_property_map{
    typedef Point value_type;
    typedef const value_type& reference;
    typedef const Point_and_Elevation& key_type;
    typedef boost::readable_property_map_tag category;
};
#endif

//typedef CGAL::Search_traits_2<Kernel> Traits;
typedef CGAL::Search_traits_2<Kernel> Traits_base;

#ifdef CGAL_OLD
typedef CGAL::Search_traits_adapter<Point_and_Elevation, My_point_property_map, Traits_base> Traits;
#else
typedef CGAL::Search_traits_adapter<Point_and_Elevation,CGAL::Nth_of_tuple_property_map<0, Point_and_Elevation>,Traits_base>    Traits;
#endif

typedef CGAL::Fuzzy_iso_box<Traits> Fuzzy_bb;
typedef CGAL::Kd_tree<Traits> Tree;


/* This class handles ALL of the nodes in a tile : 3d nodes in elevation data, 2d nodes generated from landclass, etc) */
class TGNodes {
public:

    // Constructor and destructor
    TGNodes( void )     {
        kd_tree_valid = false;
    }

    ~TGNodes( void )    {
        tg_node_list.clear();
        tg_kd_tree.clear();
    }

    // delete all the data out of node_list
    inline void clear() {
        tg_node_list.clear();
        tg_kd_tree.clear();
        kd_tree_valid = false;
    }

    // Add a point to the point list if it doesn't already exist.
    // Returns the index (starting at zero) of the point in the list.
    SGGeod unique_add( const SGGeod& p ) {
        TGNode n(p);
        n.SetFixedPosition(false);
        unsigned idx = tg_node_list.add(n);
        kd_tree_valid = false;
        
        return tg_node_list[idx].GetPosition();
    }

    SGGeod unique_add( const TGNode& n ) {
        unsigned int idx = tg_node_list.add(n);
        kd_tree_valid = false;

        return tg_node_list[idx].GetPosition();
    }

    // Add a point to the point list if it doesn't already exist
    // (checking all three dimensions.)  Returns the index (starting
    // at zero) of the point in the list.
    SGGeod unique_add_fixed_elevation( const SGGeod& p ) {
        TGNode n(p);
        n.SetFixedPosition(true);
        unsigned int idx = tg_node_list.add(n);
        kd_tree_valid = false;
        
        return tg_node_list[idx].GetPosition();
    }

    // Find the index of the specified point (compair to the same
    // tolerance as unique_add().  Returns -1 if not found.
    int find(  const SGGeod& p ) const {
        TGNode n(p);
        return tg_node_list.find(n);
    }

    void init_spacial_query( void );

    void SetElevation( int idx, double z )  { tg_node_list[idx].SetElevation( z ); }

    SGVec3f GetNormal( int idx ) const      { return tg_node_list[idx].GetNormal(); }
    void SetNormal( int idx, SGVec3f n )    { tg_node_list[idx].SetNormal( n ); }

    // return a point list of geodetic nodes
    void get_geod_nodes( std::vector<SGGeod>& points ) const;

    // Find all the nodes within a bounding box
    bool get_geod_inside( const SGGeod& min, const SGGeod& max, std::vector<SGGeod>& points ) const;

    bool get_nodes_inside( const SGGeod& min, const SGGeod& max, std::vector<TGNode*>& points ) const;
    
    // Find all the nodes on the tile edges
    bool get_geod_edge( const SGBucket& b, std::vector<SGGeod>& north, std::vector<SGGeod>& south, std::vector<SGGeod>& east, std::vector<SGGeod>& west ) const;

    // return a point list of wgs84 nodes
    void get_wgs84_nodes( std::vector<SGVec3d>& points ) const;

    // return a point list of normals
    void get_normals( std::vector<SGVec3f>& normals ) const;

    // return the ith point (constant)
    inline TGNode const& get_node( int i ) const { return tg_node_list[i]; }
    inline TGNode& get_node( int i ) { return tg_node_list[i]; }

    TGNode const& operator[]( int index ) const {
        return tg_node_list[index];
    }

    inline void AddFace( int i, unsigned int area, unsigned int poly, unsigned int tri )
    {
        tg_node_list[i].AddFace( area, poly, tri );
    }

    // return the size of the node list
    inline size_t size() const { return tg_node_list.size(); }

    void Dump( void );

    void SaveToGzFile( gzFile& fp );
    void LoadFromGzFile( gzFile& fp );
    
private:
    UniqueTGNodeSet tg_node_list;
    Tree            tg_kd_tree;
    bool            kd_tree_valid;
};

#endif // _TG_NODES_HXX
