#include "helpers.h"

#include  <time.h>
#include <iostream>                  
#include <utility>                   
#include <algorithm>
#include <map>
#include <set>
#include <limits>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/connected_components.hpp>
#include <ilcplex/ilocplex.h>
ILOSTLBEGIN

#ifdef USE_KPLS
#include <PatternAnalysis/embrex/kpls.h>
#include <itkNormalVariateGenerator.h>
#endif 

using namespace helpers;
#if defined(_MSC_VER)
#pragma warning(disable: 4996)
#endif
clock_t start_t,end_t,first_t;


#ifdef DEBUG
#define SHORT(x) (strrchr(x,'\\') ? strrchr(x,'\\')+1: x)
#define _TRACE {printf("In-%s:%d:%s:\n",SHORT(__FILE__),__LINE__,__FUNCTION__);}
#define _ETRACE {printf("Entering-%s:%d:%s:\n",SHORT(__FILE__),__LINE__,__FUNCTION__);}
#define _LTRACE {printf("Leaving-%s:%d:%s:\n",SHORT(__FILE__),__LINE__,__FUNCTION__);}
#else
#define _TRACE
#define _ETRACE
#define _LTRACE
#endif

#define TIC {start_t = clock();}
#define TOC(x) {end_t = clock(); printf("Time for %s == %0.2lfs total == %0.2lfs\n",(x),double((end_t-start_t)*1.0/CLOCKS_PER_SEC),double((end_t-first_t)*1.0/CLOCKS_PER_SEC));}

bool file_exists(char *filename)
{
	FILE * fp = fopen(filename,"r");
	if(fp!=NULL)
	{
		fclose(fp);
		return true;
	}
	return false;
}


template <typename T>
typename T::Pointer readImage(const char *filename)
{
	printf("Reading %s ... \n",filename);
	typedef typename itk::ImageFileReader<T> ReaderType;
	typename ReaderType::Pointer reader = ReaderType::New();

	ReaderType::GlobalWarningDisplayOff();
	reader->SetFileName(filename);
	try
	{
		reader->Update();
	}
	catch(itk::ExceptionObject &err)
	{
		std::cerr << "ExceptionObject caught!" <<std::endl;
		std::cerr << err << std::endl;
		//return EXIT_FAILURE;
	}
	printf("Done\n");
	return reader->GetOutput();

}
template <typename T>
int writeImage(typename T::Pointer im, const char* filename)
{
	printf("Writing %s ... \n",filename);
	typedef typename itk::ImageFileWriter<T> WriterType;

	typename WriterType::Pointer writer = WriterType::New();
	writer->SetFileName(filename);
	writer->SetInput(im);
	try
	{
		writer->Update();
	}
	catch(itk::ExceptionObject &err)
	{
		std::cerr << "ExceptionObject caught!" <<std::endl;
		std::cerr << err << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int num_files;
#define mxIsFinite(a) ((a)<1e6)

void assignmentsuboptimal1(double *assignment, double *cost, double *distMatrixIn, int nOfRows, int nOfColumns)
{
	bool infiniteValueFound, finiteValueFound, repeatSteps, allSinglyValidated, singleValidationFound;
	int n, row, col, tmpRow, tmpCol, nOfElements;
	int *nOfValidObservations, *nOfValidTracks;
	double value, minValue, *distMatrix, inf;

	inf = 1e10;

	/* make working copy of distance Matrix */
	nOfElements   = nOfRows * nOfColumns;
	distMatrix    = (double *)malloc(nOfElements * sizeof(double));
	for(n=0; n<nOfElements; n++)
		distMatrix[n] = distMatrixIn[n];

	/* initialization */
	*cost = 0;
#ifdef ONE_INDEXING
	for(row=0; row<nOfRows; row++)
		assignment[row] =  0.0;
#else
	for(row=0; row<nOfRows; row++)
		assignment[row] = -1.0;
#endif

	/* allocate memory */
	nOfValidObservations  = (int *)calloc(nOfRows,    sizeof(int));
	nOfValidTracks        = (int *)calloc(nOfColumns, sizeof(int));

	/* compute number of validations */
	infiniteValueFound = false;
	finiteValueFound  = false;
	for(row=0; row<nOfRows; row++)
		for(col=0; col<nOfColumns; col++)
			if(mxIsFinite(distMatrix[row + nOfRows*col]))
			{
				nOfValidTracks[col]       += 1;
				nOfValidObservations[row] += 1;
				finiteValueFound = true;
			}
			else
				infiniteValueFound = true;

	if(infiniteValueFound)
	{
		if(!finiteValueFound)
			return;

		repeatSteps = true;

		while(repeatSteps)
		{
			repeatSteps = false;

			/* step 1: reject assignments of multiply validated tracks to singly validated observations		 */
			for(col=0; col<nOfColumns; col++)
			{
				singleValidationFound = false;
				for(row=0; row<nOfRows; row++)
					if(mxIsFinite(distMatrix[row + nOfRows*col]) && (nOfValidObservations[row] == 1))
					{
						singleValidationFound = true;
						break;
					}

					if(singleValidationFound)
					{
						for(row=0; row<nOfRows; row++)
							if((nOfValidObservations[row] > 1) && mxIsFinite(distMatrix[row + nOfRows*col]))
							{
								distMatrix[row + nOfRows*col] = inf;
								nOfValidObservations[row] -= 1;							
								nOfValidTracks[col]       -= 1;	
								repeatSteps = true;				
							}
					}
			}

			/* step 2: reject assignments of multiply validated observations to singly validated tracks */
			if(nOfColumns > 1)			
			{	
				for(row=0; row<nOfRows; row++)
				{
					singleValidationFound = false;
					for(col=0; col<nOfColumns; col++)
						if(mxIsFinite(distMatrix[row + nOfRows*col]) && (nOfValidTracks[col] == 1))
						{
							singleValidationFound = true;
							break;
						}

						if(singleValidationFound)
						{
							for(col=0; col<nOfColumns; col++)
								if((nOfValidTracks[col] > 1) && mxIsFinite(distMatrix[row + nOfRows*col]))
								{
									distMatrix[row + nOfRows*col] = inf;
									nOfValidObservations[row] -= 1;
									nOfValidTracks[col]       -= 1;
									repeatSteps = true;								
								}
						}
				}
			}
		} /* while(repeatSteps) */

		/* for each multiply validated track that validates only with singly validated  */
		/* observations, choose the observation with minimum distance */
		for(row=0; row<nOfRows; row++)
		{
			if(nOfValidObservations[row] > 1)
			{
				allSinglyValidated = true;
				minValue = inf;
				for(col=0; col<nOfColumns; col++)
				{
					value = distMatrix[row + nOfRows*col];
					if(mxIsFinite(value))
					{
						if(nOfValidTracks[col] > 1)
						{
							allSinglyValidated = false;
							break;
						}
						else if((nOfValidTracks[col] == 1) && (value < minValue))
						{
							tmpCol   = col;
							minValue = value;
						}
					}
				}

				if(allSinglyValidated)
				{
#ifdef ONE_INDEXING
					assignment[row] = tmpCol + 1;
#else
					assignment[row] = tmpCol;
#endif
					*cost += minValue;
					for(n=0; n<nOfRows; n++)
						distMatrix[n + nOfRows*tmpCol] = inf;
					for(n=0; n<nOfColumns; n++)
						distMatrix[row + nOfRows*n] = inf;
				}
			}
		}

		/* for each multiply validated observation that validates only with singly validated  */
		/* track, choose the track with minimum distance */
		for(col=0; col<nOfColumns; col++)
		{
			if(nOfValidTracks[col] > 1)
			{
				allSinglyValidated = true;
				minValue = inf;
				for(row=0; row<nOfRows; row++)
				{
					value = distMatrix[row + nOfRows*col];
					if(mxIsFinite(value))
					{
						if(nOfValidObservations[row] > 1)
						{
							allSinglyValidated = false;
							break;
						}
						else if((nOfValidObservations[row] == 1) && (value < minValue))
						{
							tmpRow   = row;
							minValue = value;
						}
					}
				}

				if(allSinglyValidated)
				{
#ifdef ONE_INDEXING
					assignment[tmpRow] = col + 1;
#else
					assignment[tmpRow] = col;
#endif
					*cost += minValue;
					for(n=0; n<nOfRows; n++)
						distMatrix[n + nOfRows*col] = inf;
					for(n=0; n<nOfColumns; n++)
						distMatrix[tmpRow + nOfRows*n] = inf;
				}
			}
		}	
	} /* if(infiniteValueFound) */


	/* now, recursively search for the minimum element and do the assignment */
	while(true)
	{
		/* find minimum distance observation-to-track pair */
		minValue = inf;
		for(row=0; row<nOfRows; row++)
			for(col=0; col<nOfColumns; col++)
			{
				value = distMatrix[row + nOfRows*col];
				if(mxIsFinite(value) && (value < minValue))
				{
					minValue = value;
					tmpRow   = row;
					tmpCol   = col;
				}
			}

			if(mxIsFinite(minValue))
			{
#ifdef ONE_INDEXING
				assignment[tmpRow] = tmpCol+ 1;
#else
				assignment[tmpRow] = tmpCol;
#endif
				*cost += minValue;
				for(n=0; n<nOfRows; n++)
					distMatrix[n + nOfRows*tmpCol] = inf;
				for(n=0; n<nOfColumns; n++)
					distMatrix[tmpRow + nOfRows*n] = inf;			
			}
			else
				break;

	} /* while(true) */

	/* free allocated memory */
	free(nOfValidObservations);
	free(nOfValidTracks);


}

//void assignmentsuboptimal1(double *assignment, double *cost, double *distMatrixIn, int nOfRows, int nOfColumns)
#define USE_VNL_HUNGARIAN 
vcl_vector<unsigned int> getTimeAssociations(std::vector<FeaturesType> &a,std::vector<FeaturesType> &b)
{

#define DEBUG_RANK -1
	int rows =0;
	int cols =0;
	rows = a.size();
	cols = b.size();
	printf("Rows = %d Cols = %d\n",rows,cols);
	double overlap;
#ifdef USE_VNL_HUNGARIAN
	vnl_matrix<double> mat(rows,cols);
	printf("Allocated Rows = %d Cols = %d\n",mat.rows(),mat.cols());
	//int pa =0; 
	//int pb =0;
	for(int cr = 0; cr<rows; cr++)
	{

		bool enforce_overlap = false;
		for(int cc=0; cc<cols; cc++)
		{
			overlap = features_box_overlap(a[cr],b[cc]);
			if(overlap>1) // volume of overlap > 1
			{
				enforce_overlap = true;
				break;
			}
		}
		/*	if(rank==DEBUG_RANK)
		{
		if(enforce_overlap)
		printf("%d/%d: I did enforce overlap for %d\n",rank,npes,cr);
		else
		printf("%d/%d: I did not enforce overlap for %d\n",rank,npes,cr);
		}*/
		for(int cc =0; cc<cols; cc++)
		{
			mat(cr,cc) = features_diff(a[cr],b[cc],enforce_overlap);
		}
	}
	printf("About to call vnl_hungarian_algorithm\n");
	vcl_vector<unsigned int> ret = vnl_hungarian_algorithm(mat);
	printf("Returned from vnl_hungarian_algorithm\n");
	for(unsigned int counter=0; counter< ret.size(); counter++)
	{
		if(mxIsFinite(ret[counter]))
		{
			if(!mxIsFinite(mat(counter,ret[counter])))
			{
				ret[counter] = static_cast<unsigned int>(-1);
			}
		}
	}
	printf("Returning from getTimeAssociations\n");
	return ret;
#else
	double * assignment  = (double*) malloc(rows*sizeof(double));
	double * cost = (double*) malloc(sizeof(double));
	double * distMatrixIn = (double*) malloc(rows *cols*sizeof(double));

	if(assignment == NULL)
		printf("Couldn't allocate memory assignment\n");

	if(distMatrixIn == NULL)
		printf("Couldn't allocate memory for distMatrixIn\n");

	if(cost == NULL)
		printf("Couldn't allocate memory for cost\n");

	printf("%d/%d About to assign datamatrix values\n",rank,npes);
	int pa =0; 
	int pb =0;
	for(int cr = 0; cr<rows; cr++)
	{

		//if(rank==0)
		//{
		//	printf("0/124: bbox %d %d %d %d %d %d \n",a[cr].bbox.sx,a[cr].bbox.sy,a[cr].bbox.sz,a[cr].bbox.ex,a[cr].bbox.ey,a[cr].bbox.ez);
		//}
		bool enforce_overlap = false;
		for(int cc=0; cc<cols; cc++)
		{
			overlap = features_box_overlap(a[cr],b[cc]);
			if(overlap>1) // volume of overlap > 1
			{
				enforce_overlap = true;
				break;
			}
		}
		//if(rank==DEBUG_RANK)
		{
			if(enforce_overlap)
				printf("%d/%d: I did enforce overlap for %d\n",rank,npes,cr);
			else
				printf("%d/%d: I did not enforce overlap for %d\n",rank,npes,cr);
		}
		for(int cc =0; cc<cols; cc++)
		{
			distMatrixIn[cr+rows*cc] = features_diff(a[cr],b[cc],enforce_overlap);
			//if(rank==DEBUG_RANK)
			{
				//	if(cr==0)
				{
					printf("distmatrix[%d,%d]=%0.3lf\n",cr,cc,distMatrixIn[cr+rows*cc]);
				}
			}
		}

	}
	printf("%d/%d About to call assignmentsuboptimal1\n",rank,npes);
	assignmentsuboptimal1(assignment,cost,distMatrixIn,rows,cols);
	printf("%d/%d Exited assignmentsuboptimal1\n",rank,npes);
	vcl_vector<unsigned int> vec(rows);
	int assigned_count=0;
	//	if(rank==DEBUG_RANK)
	//	{
	//		for(int counter=0; counter< rows; counter++)
	//		{
	//			printf("%d/%d assignment[%d] = %0.3lf\n",rank,npes,counter,assignment[counter]);
	//		}
	//	}
	printf("%d/%d About to start assigning vec values\n",rank,npes);
	for(int cr = 0; cr<rows; cr++)
	{
		if(assignment[cr]>-0.1)
		{
			vec[cr]=static_cast<unsigned int>(assignment[cr]+0.5);
			//			if(rank==DEBUG_RANK)
			//				printf("%d/%d : assigned_nums[%d] = %d\n",rank,npes,cr,vec[cr]);
			assigned_count++;
		}
		else
		{
			vec[cr]=static_cast<unsigned int>(-1);
		}
	}
	//	if(rank==DEBUG_RANK)
	//	{
	//		printf("%d/%d: assigned_count = %d\n",rank,npes,assigned_count);
	//	}
	//free(assignment); FIXME : was giving glibc corruption errors;
	//free(cost);
	//free(distMatrixIn);
	return vec;

#endif

}

void getArrayFromStdVector(std::vector<FeaturesType> &f, FeaturesType	*&farray)
{
	farray = new FeaturesType[f.size()];
	for(unsigned int counter=0; counter<f.size(); counter++)
	{
		farray[counter]=f[counter];
	}
}

void getStdVectorFromArray(FeaturesType *farray, int n,std::vector<FeaturesType> &f)
{
	f.reserve(n);
	for(int counter=0; counter<n; counter++)
	{
		f.push_back(farray[counter]);
	}
}




//#define CACHE_PREFIX "D:/ucb dataset/output/ena/cache"
#define CACHE_PREFIX "cache"
#define MAX_TIME 200
#define MAX_TAGS 4
#define MAX_LABEL 10000
#define VESSEL_CHANNEL 4 // FIXME : make it dynamic based on user input
#define PAUSE {printf("%d:>",__LINE__);scanf("%*d");}

bool compare(FeaturesType a, FeaturesType b)
{
	return a.time<b.time;
}
void createTrackFeatures(std::vector<FeaturesType> fvector[MAX_TIME][MAX_TAGS], std::vector<ftk::TrackFeatures> &tfs, int c,int num_t)
{
	int max_track_num = 0;
	for(int t = 0; t< num_t; t++)
	{
		for(unsigned int counter=0; counter< fvector[t][c-1].size(); counter++)
		{
			max_track_num = MAX(max_track_num,fvector[t][c-1][counter].num);
		}
	}

	for(int counter=1; counter <= max_track_num; counter++)
	{
		ftk::TrackFeatures trackf;
		trackf.intrinsic_features.clear();
		for(int t = 0; t< num_t;t++)
		{
		for(unsigned int counter1 = 0; counter1 < fvector[t][c-1].size(); counter1++)
		{
			if(fvector[t][c-1][counter1].num == counter)
			{
				trackf.intrinsic_features.push_back(fvector[t][c-1][counter1]);
			}
		}
		}
		std::sort(trackf.intrinsic_features.begin(),trackf.intrinsic_features.end(),compare);
		tfs.push_back(trackf);
		//PRINTF("Added %d elements to tfs\n",counter);
	}
}


struct FeatureVariances{
	enum ScalarTypes
	{
		VOLUME, INTEGRATED_INTENSITY, ECCENTRICITY, ELONGATION, ORIENTATION, BBOX_VOLUME, \
		SUM, MEAN, MEDIAN, MINIMUM, MAXIMUM, SIGMA, VARIANCE, \
		SURFACE_GRADIENT, INTERIOR_GRADIENT, SURFACE_INTENSITY, INTERIOR_INTENSITY, \
		INTENSITY_RATIO, RADIUS_VARIATION, SURFACE_AREA, SHAPE, SHARED_BOUNDARY, \
		SKEW, ENERGY, ENTROPY,
		T_ENERGY, T_ENTROPY, INVERSE_DIFFERENCE_MOMENT, INERTIA, CLUSTER_SHADE, CLUSTER_PROMINENCE,
	
	};	//FEATURES WILL GET ASSIGNED INT 0,1,...N-1

	static const int N = CLUSTER_PROMINENCE + 1;
	float variances[N];
	int Dimensions;

	float bounds[6]; // start_x end_x start_y end_y start_z end_z
	float distVariance;
	float spacing[3];
	float timeVariance;
	float overlapVariance;
};



class CellTracker{
	
	struct TrackVertex{
		bool special;
		int t;
		int findex;
		int new_label;
	};

	struct TrackEdge{
		int utility;
		bool coupled;
		bool fixed;
		bool selected;
	};


	struct MergeCandidate{
		int t;
		int index1, index2;
		FeaturesType f;
	};
public:
	typedef boost::adjacency_list<boost::listS, boost::vecS, boost::bidirectionalS, TrackVertex, TrackEdge> TGraph;
	typedef std::vector< std::vector < FeaturesType> > VVF;
	typedef std::vector< std::vector<TGraph::vertex_descriptor> > VVV;
	typedef std::vector< std::vector < MergeCandidate > > VVM;
	typedef std::vector< std::vector < LabelImageType::Pointer > > VVL;
	typedef std::vector< std::vector < InputImageType::Pointer > > VVR;

	CellTracker(VVF &fv, FeatureVariances &fvariances, VVL &l, VVR &r)
	{
		fvector  = fv;
		old_to_new.resize(fv.size());
		fvar = fvariances;
		limages = l; // copy only pointers
		rimages = r;
		UTILITY_MAX = 1<<16;
		K = 2;
	}

	void run();
	void writeGraphViz(char * filename);
	void writeGraphML (char * filename);
	std::vector<std::map<int,int>> old_to_new;
private:
	
	float overlap(float bb1[6],float bb2[6]);
	float get_boundary_dist(float x[3]);
	int compute_boundary_utility(float x[3]);
	float get_distance( float x1[3],float x2[3]);
	FeaturesType get_merged_features(int, int,int);
	int add_disappear_vertices(int t);
	int add_appear_vertices(int t);
	int add_normal_edges(int tmin, int tmax);
	void populate_merge_candidates(int t);
	int add_merge_split_edges(int tmax);
	void solve_lip(void);
	void solve_qip(void);
	void prune(int);
	int compute_normal_utility(FeaturesType f1, FeaturesType f2);
	void print_stats(void);

	//variables

	TGraph g; 
	VVF fvector; 
	VVL limages;
	VVR rimages;
	VVV rmap;
	VVM m_cand;
	std::map<TGraph::edge_descriptor, TGraph::edge_descriptor> coupled_map;
	int UTILITY_MAX;
	FeatureVariances fvar;
	int K;
	

};



float CellTracker::overlap(float bb1[6],float bb2[6])
{
	float sx,sy,sz;
	float ex,ey,ez;
	sx = MAX(bb1[0],bb2[0]);
	sy = MAX(bb1[2],bb2[2]);
	sz = MAX(bb1[4],bb2[4]);
	ex = MIN(bb1[1],bb2[1]);
	ey = MIN(bb1[3],bb2[3]);
	ez = MIN(bb1[5],bb2[5]);

	float overlap=0;
	if((sx<ex) && (sy<ey) && (sz<ez))
	{
		overlap = (ex-sx)*(ey-sy)*(ez-sz);
	}

	return overlap;
}
float CellTracker::get_boundary_dist(float x[3])
{
	float minv = x[0]*fvar.spacing[0];
	minv = MIN(minv,x[1]*fvar.spacing[1]);
	minv = MIN(minv,x[2]*fvar.spacing[2]);
	minv = MIN(minv, (fvar.bounds[1] - x[0])*fvar.spacing[0]);
	minv = MIN(minv, (fvar.bounds[3] - x[1])*fvar.spacing[1]);
	minv = MIN(minv, (fvar.bounds[5] - x[2])*fvar.spacing[2]);
//	printf("Boundary dist = %f %f %f %f\n", minv,x[0],x[1],x[2]);
	return minv;
}

int CellTracker::compute_boundary_utility(float x[3])
{
	float dist = get_boundary_dist(x);
	return UTILITY_MAX*(exp(-dist*dist/2.0/fvar.distVariance))/1000.0;
}

int CellTracker::compute_normal_utility(FeaturesType f1, FeaturesType f2)
{
	float utility = 0;
	for(int counter=0; counter< FeaturesType::N; counter++)
	{
		utility += (f1.ScalarFeatures[counter]-f2.ScalarFeatures[counter])*(f1.ScalarFeatures[counter]-f2.ScalarFeatures[counter])/fvar.variances[counter];
	}
	float dist = get_distance(f1.Centroid,f2.Centroid);
	utility += dist*dist/fvar.distVariance;
	utility += (abs(f1.time-f2.time)-1)*(abs(f1.time-f2.time)-1)/fvar.timeVariance;
	float ovlap = overlap(f1.BoundingBox,f2.BoundingBox);
	utility += ovlap*ovlap/fvar.overlapVariance;
	utility /= 2.0;
	utility = UTILITY_MAX*(exp(-utility));
	return utility;
}

float CellTracker::get_distance( float x1[3],float x2[3])
{
	float dist  = 0;
	dist += (x1[0]-x2[0])*(x1[0]-x2[0])*fvar.spacing[0]*fvar.spacing[0];
	dist += (x1[1]-x2[1])*(x1[1]-x2[1])*fvar.spacing[1]*fvar.spacing[1];
	dist += (x1[2]-x2[2])*(x1[2]-x2[2])*fvar.spacing[2]*fvar.spacing[2];
	dist = sqrt(dist);
	return dist;
}
int CellTracker::add_disappear_vertices(int t)
{
	_ETRACE;
	using boost::graph_traits;
	typedef graph_traits<typename TGraph>::vertex_iterator vertex_iter;
	TGraph::vertex_descriptor v;
	TGraph::edge_descriptor e;

	vertex_iter vi, vend;
	bool added;
	int ret_count = 0;
	for(tie(vi,vend) = vertices(g); vi != vend; ++vi)
	{
		//printf("hi t = %d\n",t);
		if(g[*vi].t == t-1)
		{
			//printf("hi 1\n");
			if(g[*vi].special == 0)
			{
				//printf("hi 2\n");
				if(get_boundary_dist(fvector[t-1][g[*vi].findex].Centroid) < 4.0*sqrt(fvar.distVariance))
				{
					v = add_vertex(g);
					g[v].special = 1;
					g[v].t = t;
					tie(e,added) = add_edge(*vi,v,g);
					if(added)
					{
						g[e].coupled = 0;
						g[e].fixed = 0;
						g[e].selected = 0;
						g[e].utility = compute_boundary_utility(fvector[t-1][g[*vi].findex].Centroid);
						ret_count ++;
					}
				}
			}
		}
	}
	_LTRACE;
	return ret_count;
}

int CellTracker::add_appear_vertices(int t)
{
	_ETRACE;
	using boost::graph_traits;
	typedef graph_traits<typename TGraph>::vertex_iterator vertex_iter;
	TGraph::vertex_descriptor v;
	TGraph::edge_descriptor e;

	vertex_iter vi, vend;
	bool added;
	int ret_count = 0;
	for(tie(vi,vend) = vertices(g); vi != vend; ++vi)
	{
		if(g[*vi].t == t+1)
		{
			if(g[*vi].special == 0)
			{
				//printf("findex = %d fvector[%d].size() = %d\n",g[*vi].findex,t,fvector[t].size());
				if(get_boundary_dist(fvector[t+1][g[*vi].findex].Centroid) <  4.0*sqrt(fvar.distVariance))
				{
					_TRACE;
					v = add_vertex(g);
					g[v].special = 1;
					g[v].t = t;

					tie(e,added) = add_edge(v,*vi,g);
					if(added)
					{
						g[e].coupled = 0;
						g[e].fixed = 0;
						g[e].selected = 0;
						g[e].utility = compute_boundary_utility(fvector[t+1][g[*vi].findex].Centroid);
						ret_count ++;
					}
					_TRACE;
				}
			}
		}
	}
	_LTRACE;
	return ret_count;
}

int CellTracker::add_normal_edges(int tmin, int tmax)
{
	_ETRACE;
	TGraph::edge_descriptor e;
	bool added = false;
	int nec = 0;
	float epsilon = 50;
	int tried1 =0,tried2 = 0 ;
	for(int counter=0; counter < fvector[tmax].size(); counter++)
	{
		// for every vertex, find the correspondences in the previous frames
		TGraph::vertex_descriptor v = rmap[tmax][counter];
		if(TGraph::null_vertex() != v) // do we have a non-null vertex? then ...
		{
			tried1++;
			for(int t = tmin; t < tmax; ++t)
			{
				
				for(int counter1 = 0; counter1 < fvector[t].size(); counter1++)
				{
					tried2++;
					float dist = get_distance(fvector[t][counter1].Centroid,fvector[tmax][counter].Centroid);
					if(dist < 4.0*sqrt(fvar.distVariance))
					{
						//add the edge
						tie(e,added) = add_edge(rmap[t][counter1],v,g);
						if(added)
						{
							g[e].coupled = 0;
							g[e].fixed = 0;
							g[e].selected = 0;
							g[e].utility = compute_normal_utility(fvector[t][counter1],fvector[tmax][counter]);
							nec++;
						}
					}
				}
			}
		}
	}
	_LTRACE;
	printf("add_normal_edges: I tried %d %d\n",tried1, tried2);
	return nec;
}

FeaturesType CellTracker::get_merged_features(int t1, int i1, int i2)
{
	int t2 = t1;
	LabelImageType::Pointer p1,p2;
	InputImageType::Pointer r1,r2;
	p1 = limages[t1][i1];
	p2 = limages[t2][i2];
	r1 = rimages[t1][i1];
	r2 = rimages[t2][i2];


	LabelImageType::SizeType ls;
	
	int lbounds[6];

	lbounds[0] = MIN(fvector[t1][i1].BoundingBox[0],fvector[t2][i2].BoundingBox[0]);
	lbounds[2] = MIN(fvector[t1][i1].BoundingBox[2],fvector[t2][i2].BoundingBox[2]);
	lbounds[4] = MIN(fvector[t1][i1].BoundingBox[4],fvector[t2][i2].BoundingBox[4]);
	lbounds[1] = MAX(fvector[t1][i1].BoundingBox[1],fvector[t2][i2].BoundingBox[1]);
	lbounds[3] = MAX(fvector[t1][i1].BoundingBox[3],fvector[t2][i2].BoundingBox[3]);
	lbounds[5] = MAX(fvector[t1][i1].BoundingBox[5],fvector[t2][i2].BoundingBox[5]);

	ls[0] = lbounds[1]-lbounds[0]+1;
	ls[1] = lbounds[3]-lbounds[2]+1;
	ls[2] = lbounds[5]-lbounds[4]+1;
	
	LabelImageType::Pointer p = LabelImageType::New();
	InputImageType::Pointer r = InputImageType::New();
	LabelImageType::IndexType lindex;
	lindex.Fill(0);
	LabelImageType::RegionType lregion;
	lregion.SetIndex(lindex);
	lregion.SetSize(ls);
	p->SetRegions(lregion);
	p->Allocate();
	p->FillBuffer(0);
	r->SetRegions(lregion);
	r->Allocate();
	r->FillBuffer(0);
	LabelIteratorType liter1(p1,p1->GetLargestPossibleRegion());
	IteratorType riter1(r1,r1->GetLargestPossibleRegion());


	lindex[0] = fvector[t1][i1].BoundingBox[0]-lbounds[0];
	lindex[1] = fvector[t1][i1].BoundingBox[2]-lbounds[2];
	lindex[2] = fvector[t1][i1].BoundingBox[4]-lbounds[4];

	lregion.SetSize(p1->GetLargestPossibleRegion().GetSize());
	lregion.SetIndex(lindex);

	LabelIteratorType liter(p,lregion);
	IteratorType riter(r,lregion);
	for(liter1.GoToBegin(),riter1.GoToBegin(),liter.GoToBegin(),riter.GoToBegin();!liter1.IsAtEnd(); ++liter1,++riter1,++liter,++riter)
	{
		if(liter1.Get()==fvector[t1][i1].num)
			liter.Set(255);
		riter.Set(riter1.Get());
	}
	
	LabelIteratorType liter2(p2,p2->GetLargestPossibleRegion());
	IteratorType riter2(r2,r2->GetLargestPossibleRegion());

	lindex[0] = fvector[t2][i2].BoundingBox[0]-lbounds[0];
	lindex[1] = fvector[t2][i2].BoundingBox[2]-lbounds[2];
	lindex[2] = fvector[t2][i2].BoundingBox[4]-lbounds[4];
	lregion.SetIndex(lindex);
	lregion.SetSize(p2->GetLargestPossibleRegion().GetSize());
	
	liter = LabelIteratorType(p,lregion);
	riter = IteratorType(r,lregion);

	for(liter2.GoToBegin(),riter2.GoToBegin(),liter.GoToBegin(),riter.GoToBegin();!liter2.IsAtEnd(); ++liter2,++liter,++riter,++riter2)
	{
		if(liter2.Get()==fvector[t2][i2].num)
			liter.Set(255);
		riter.Set(riter2.Get());
	}


	std::vector<FeaturesType> f1;
	getFeatureVectorsFarsight(p,r,f1,t1,fvector[t1][i1].tag);

	FeaturesType f = f1[0];
	f.Centroid[0]+=lbounds[0];
	f.Centroid[1]+=lbounds[2];
	f.Centroid[2]+=lbounds[4];
	f.WeightedCentroid[0]+=lbounds[0];
	f.WeightedCentroid[2]+=lbounds[2];
	f.WeightedCentroid[4]+=lbounds[4];
	f.BoundingBox[0]+=lbounds[0];
	f.BoundingBox[1]+=lbounds[0];
	f.BoundingBox[2]+=lbounds[2];
	f.BoundingBox[3]+=lbounds[2];
	f.BoundingBox[4]+=lbounds[4];
	f.BoundingBox[5]+=lbounds[5];
	return f;
}
void  CellTracker::populate_merge_candidates(int t)
{
	MergeCandidate m;
	std::vector<MergeCandidate> vm;
	std::vector<MergeCandidate> nullvm;
	nullvm.clear();
	vm.clear();
	for(int counter = 0; counter < fvector[t].size(); counter++)
	{
		for(int counter1 = counter+1; counter1 < fvector[t].size(); counter1++)
		{
			if( MIN(fvector[t][counter].ScalarFeatures[FeaturesType::BBOX_VOLUME],fvector[t][counter1].ScalarFeatures[FeaturesType::BBOX_VOLUME])/overlap(fvector[t][counter].BoundingBox,fvector[t][counter1].BoundingBox)< 4.0*sqrt(fvar.overlapVariance))
			{
				m.t = t;
				m.index1 = counter;
				m.index2 = counter1;
				m.f = get_merged_features(t,counter,counter1);
				vm.push_back(m);
			}
		}
	}
	while(m_cand.size()<t)
	{
		m_cand.push_back(nullvm); //CHECK
	}
	m_cand.push_back(vm);
}


int CellTracker::add_merge_split_edges(int tmax)
{
	_TRACE
	int msec = 0;
	if(m_cand.size() < tmax)
	{
		printf("something is wrong.. plz check to make sure m_cand are populated correctly\n");
		_exit(1);
	}
	if(m_cand.size() < tmax + 1)
	{
		populate_merge_candidates(tmax);
	}
	
	TGraph::edge_descriptor e1,e2;
	bool added1, added2;
	for(int tcounter = MAX(tmax-K,0);tcounter <=tmax-1; tcounter++)
	{
	for(int counter=0; counter< m_cand[tcounter].size(); counter++)
	{
		for(int counter1 = 0; counter1 < fvector[tmax].size(); counter1++)
		{
			int i1 = m_cand[tcounter][counter].index1;
			int i2 = m_cand[tcounter][counter].index2;
			
			if(get_distance(m_cand[tcounter][counter].f.Centroid,fvector[tmax][counter1].Centroid)<4.0*sqrt(fvar.distVariance))
			{
				int utility = compute_normal_utility(m_cand[tcounter][counter].f,fvector[tmax][counter1]);
				tie(e1,added1) = add_edge(rmap[tcounter][i1],rmap[tmax][counter1],g);
				tie(e2,added2) = add_edge(rmap[tcounter][i2],rmap[tmax][counter1],g);
				if(added1&&added2)
				{
					g[e1].coupled = 1;
					g[e2].coupled = 1;
					coupled_map[e1] = e2;
					coupled_map[e2] = e1;
					g[e1].utility = utility/2;
					g[e2].utility = utility/2;
					g[e1].fixed = 0;
					g[e2].fixed = 0;
					g[e1].selected = 0;
					g[e2].selected = 0;
					msec+=2;
				}
			}
		}
	}
	}
	for(int counter=0; counter< m_cand[tmax].size(); counter++)
	{
		for(int tcounter = MAX(tmax-K,0);tcounter <=tmax-1; tcounter++)
		{		
			for(int counter1 = 0; counter1 < fvector[tcounter].size(); counter1++)
			{
				int i1 = m_cand[tmax][counter].index1;
				int i2 = m_cand[tmax][counter].index2;
				if(get_distance(m_cand[tmax][counter].f.Centroid,fvector[tcounter][counter1].Centroid)<4.0*sqrt(fvar.distVariance))
				{
					int utility = compute_normal_utility(m_cand[tmax][counter].f,fvector[tcounter][counter1]);
					tie(e1,added1) = add_edge(rmap[tmax][i1],rmap[tcounter][counter1],g);
					tie(e2,added2) = add_edge(rmap[tmax][i2],rmap[tcounter][counter1],g);
					if(added1&&added2)
					{
						g[e1].coupled = 1;
						g[e2].coupled = 1;
						coupled_map[e1] = e2;
						coupled_map[e2] = e1;
						g[e1].utility = utility/2;
						g[e2].utility = utility/2;
						g[e1].fixed = 0;
						g[e2].fixed = 0;
						g[e1].selected = 0;
						g[e2].selected = 0;
						msec+=2;
					}
				}
			}
		}
	}
	return msec;
}
void CellTracker::solve_qip()
{
	
}
void CellTracker::solve_lip()
{
	IloEnv env;
	try{
		using boost::graph_traits;
		graph_traits<TGraph>::edge_iterator ei,eend;
		std::map<TGraph::edge_descriptor,int> var_index;
		std::map<int,TGraph::edge_descriptor> inv_index;
		std::vector<int> utility;

		IloObjective obj = IloMaximize(env);
		IloRangeArray c(env);

		//find number of variables
		tie(ei,eend) = boost::edges(g);
		int ecount = 0;
		int varc = 0;
		for(;ei != eend; ++ei)
		{
			ecount++;
			if(g[*ei].fixed != 1)
			{
				if(g[*ei].selected!=1)
				{
					var_index[*ei] = varc;
					inv_index[varc] = *ei;
					g[*ei].selected = 1;
					if(g[*ei].coupled == 1)
					{
						var_index[coupled_map[*ei]] = varc;
						inv_index[varc] = coupled_map[*ei];
						g[coupled_map[*ei]].selected = 1;
					}
					utility.push_back(g[*ei].utility);
					varc++;
				}
			}
		}
		printf("ecount = %d varc = %d\n",ecount,varc);
		graph_traits<TGraph>::vertex_iterator vi,vend;

		IloBoolVarArray x(env,varc);

		for(int counter=0; counter < varc; counter++)
		{
			obj.setLinearCoef(x[counter],utility[counter]);
			printf("utility[%d] = %d\n",counter,utility[counter]);
		}

		int vcount = -1;
		for(tie(vi,vend) = vertices(g); vi != vend; ++vi)
		{

			graph_traits<TGraph>::in_edge_iterator e_i,e_end;
			tie(e_i,e_end) = in_edges(*vi,g);
			bool once = false;
			float fixed_sum = 0;
			for(;e_i!=e_end; ++e_i)
			{
				if(g[*e_i].fixed == 0 )
				{
					if(once == false )
					{
						once = true;
						vcount++;
						c.add(IloRange(env,0,1));
					}
					c[vcount].setLinearCoef(x[var_index[*e_i]],1.0);
				}
				else
				{
					if(g[*e_i].coupled==0)
						fixed_sum += g[*e_i].selected;
					else
						fixed_sum += 0.5*g[*e_i].selected;
				}
			}
			if(once)
				c[vcount].setBounds(0,1-fixed_sum);
			graph_traits<TGraph>::out_edge_iterator e_2,e_end2;
			tie(e_2,e_end2) = out_edges(*vi,g);
			once = false;
			fixed_sum = 0;
			for(;e_2!=e_end2; ++e_2)
			{
				if(g[*e_2].fixed == 0 )
				{
					if(once == false )
					{
						once = true;
						vcount++;
						c.add(IloRange(env,0,1));
					}
					c[vcount].setLinearCoef(x[var_index[*e_2]],1.0);
				}
				else
				{
					if(g[*e_2].coupled==0)
						fixed_sum += g[*e_2].selected;
					else
						fixed_sum += 0.5*g[*e_2].selected;
				}
			}
			if(once)
				c[vcount].setBounds(0,1-fixed_sum);

		}
		printf("vcount = %d\n",vcount);
		//scanf("%*d");
		IloModel model(env);
		model.add(obj);
		model.add(c);


		//std::cout<<model<<std::endl;
		IloCplex cplex(model);
		if(!cplex.solve())
		{
			std::cerr << " Could not solve.. error"<<std::endl;
		}
		IloNumArray vals(env);
		env.out() << "Solution status = " << cplex.getStatus() << std::endl;
		env.out() << "Solution value  = " << cplex.getObjValue() << std::endl;
		cplex.getValues(vals, x);
		//env.out() << "Values        = " << vals << std::endl;
		std::cout << "Values.getSize() = "<< vals.getSize() << std::endl;
		for(int counter=0; counter< vals.getSize(); counter++)
		{
			//assert(g[inv_index[counter]].selected == 1);
			g[inv_index[counter]].selected = vals[counter];
			if(g[inv_index[counter]].coupled==1)
				g[coupled_map[inv_index[counter]]].selected = vals[counter];
			if(vals[counter]==1 && utility[counter] <0)
			{
				printf("wrong @ %d\n",counter);
			}
		}
	}
	catch(IloException &e)
	{
		std::cerr << e << std::endl;
	}
	env.end();
	//scanf("%*d");
}

void CellTracker::prune(int t)
{


	using boost::graph_traits;
	graph_traits<TGraph>::edge_iterator e_i,e_end;
	tie(e_i,e_end) = edges(g);
	for(;e_i!=e_end; ++e_i)
	{
		g[*e_i].selected = 0;
	}
	
}
void CellTracker::print_stats()
{
	using boost::graph_traits;

	graph_traits<TGraph>::edge_iterator e_i,e_end;

	float sne,sde,sae,sme;
	sne=sde=sae=sme=0;

	FILE*fp = fopen("C:\\Users\\arun\\Research\\Tracking\\harvard\\debug.txt","w");
	if(fp==NULL)
	{
		printf("Could not open debug.txt \n");
		_exit(-1);
	}
	tie(e_i,e_end) = edges(g);
	for(;e_i!=e_end; ++e_i)
	{
		if(g[*e_i].selected == 1)
		{
			if(g[*e_i].coupled == 1)
				sme += 0.5;
			else if(g[source(*e_i,g)].special == 1)
				sae++;
			else if(g[target(*e_i,g)].special == 1)
				sde++;
			else
				sne++;
		}
	}
	boost::graph_traits<TGraph>::vertex_iterator v_i,v_end;

	char mod;
	for(tie(v_i,v_end)=vertices(g); v_i != v_end; ++v_i)
	{
		boost::graph_traits<TGraph>::in_edge_iterator e_in,e_in_end;
		tie(e_in,e_in_end) = in_edges(*v_i,g);
		bool once = false;
		for(;e_in!=e_in_end;++e_in)
		{
			once = true;
			TrackVertex v = g[source(*e_in,g)];
			if(g[*e_in].selected == 1)
				mod = '*';
			else
				mod = ' ';
			if(v.special == 0)
				fprintf(fp,"[%d].%d%c ",v.t,fvector[v.t][v.findex].num,mod);
			else
				fprintf(fp,"A ");
		}

		if(g[*v_i].special == 0)
			fprintf(fp," -> [%d].%d ->",g[*v_i].t,fvector[g[*v_i].t][g[*v_i].findex].num);
		else if (once == false)
			fprintf(fp,"A ->");
		else
			fprintf(fp,"-> D");
		boost::graph_traits<TGraph>::out_edge_iterator e_out,e_out_end;
		tie(e_out,e_out_end) = out_edges(*v_i,g);
		for(;e_out!=e_out_end;++e_out)
		{
			TrackVertex v = g[target(*e_out,g)];
			if(g[*e_out].selected == 1)
				mod = '*';
			else
				mod = ' ';
			if(v.special == 0)
				fprintf(fp,"[%d].%d%c ",v.t,fvector[v.t][v.findex].num,mod);
			else
				fprintf(fp,"D ");
		}
		fprintf(fp,"\n");
	}
	fclose(fp);
	printf("Normal = %d, Appear = %d, Disappear = %d, Merge/split = %d\n",int(sne+0.5),int(sae+0.5),int(sde+0.5),int(sme+0.5));



	PAUSE;
}
void CellTracker::run()
{

	TGraph::vertex_descriptor v1 = TGraph::null_vertex();

	std::vector< TGraph::vertex_descriptor > vv;
	for(int counter=0; counter< fvector.size(); counter++)
	{
		vv.clear();
		for(int counter1 = 0; counter1 < fvector[counter].size(); counter1++)
		{
			vv.push_back(v1);
		}
		rmap.push_back(vv);
	}

	int avc = 0;
	int dvc = 0;
	int nec = 0;
	int msec = 0;

	TGraph::vertex_descriptor v;

	for(int counter=0; counter< fvector[0].size(); counter++)
	{
		v = add_vertex(g);
		g[v].special = 0;
		g[v].t = 0;
		g[v].findex = counter;
		rmap[0][counter] = v;
	}
	populate_merge_candidates(0);


	_TRACE;
	first_t = clock();
	for(int t = 1; t < fvector.size(); t++)
	{
		int tmin = MAX(0,t-K);
		for(int counter=0; counter< fvector[t].size(); counter++)
		{
			v = add_vertex(g);
			g[v].special = 0;
			g[v].t = t;
			g[v].findex = counter;
			rmap[t][counter] = v;
		}
		//printf("T = %d\n",t);
TIC		populate_merge_candidates(t);//TOC("populate_merge_candidates");
TIC;		nec += add_normal_edges(tmin,t);// TOC("add_normal_edges()");
TIC;		msec+= add_merge_split_edges(t);// TOC("add_merge_split_edges()");
TIC;		dvc += add_disappear_vertices(t);// TOC("add_disappear_vertices()");
TIC;		avc += add_appear_vertices(t-1);// TOC("add_appear_vertices()");
		printf("total edges = %d+%d+%d+%d = %d\n",nec,dvc,avc,msec,nec+dvc+avc+msec);
		//PAUSE;
TIC;		prune(t);//TOC("prune()");
	}
	solve_lip();
	print_stats();
	
	boost::graph_traits<TGraph>::edge_iterator e_i,e_next,e_end;
	for(tie(e_i,e_end) = edges(g); e_i!= e_end ; e_i = e_next)
	{
		e_next = e_i;
		++e_next;
		if(g[*e_i].selected == 0)
		{

			remove_edge(*e_i,g);
		}
	}

	std::vector<int> component(num_vertices(g));
	int num = connected_components(g,&component[0]);
	int tempnum = num;

	std::vector<int> vertex_count(num);
	for(int counter=0; counter < vertex_count.size(); counter++)
	{
		vertex_count[counter]=0;
	}
	boost::graph_traits<TGraph>::vertex_iterator v_i,v_end;
	tie(v_i,v_end) = vertices(g);
	boost::property_map<TGraph, boost::vertex_index_t>::type index;
	index = get(boost::vertex_index,g);
	for(;v_i!=v_end; ++v_i)
	{
		vertex_count[component[index[*v_i]]]++;
	}
	tie(v_i,v_end) = vertices(g);
	for(;v_i!=v_end; ++v_i)
	{
		if(vertex_count[component[index[*v_i]]]==1 && g[*v_i].special == 1)
		{
			tempnum--;
		}
		else
		{
			printf("%d ",component[index[*v_i]]);
		}
	}
	tie(v_i,v_end) = vertices(g);

	int max1 = -1;
	for(;v_i!=v_end;++v_i)
	{
		if(g[*v_i].special == 0 )
		{
			old_to_new[g[*v_i].t][fvector[g[*v_i].t][g[*v_i].findex].num]=component[index[*v_i]]+1;
			max1 = MAX(max1,component[index[*v_i]]+1);
		}

	}

	printf("number of connected components = %d reduced = %d maxcomp = %d\n",num,tempnum,max1);

}



void CellTracker::writeGraphViz(char * filename)
{
	std::ofstream f;
	f.open(filename,std::ios::out);
	write_graphviz(f,g);
	f.close();
}

void CellTracker::writeGraphML(char * filename)
{
	std::ofstream f;
	f.open(filename,std::ios::out);
	using boost::dynamic_properties;

	dynamic_properties dp;
	dp.property("time",get(&TrackVertex::t,g));
	dp.property("special",get(&TrackVertex::special,g));
	dp.property("utility",get(&TrackEdge::utility,g));
	dp.property("coupled",get(&TrackEdge::coupled,g));
	write_graphml(f,g,dp,true);
	f.close();
}

void testKPLS()
{
	KPLS * kpls = new KPLS();
	kpls->SetLatentVars(5);
	kpls->SetSigma(20);

	int num_rows = 1000;
	int num_cols = 1;

	itk::Statistics::NormalVariateGenerator::Pointer nvg1, nvg2;
	nvg1 = itk::Statistics::NormalVariateGenerator::New();
	nvg2 = itk::Statistics::NormalVariateGenerator::New();

	nvg1->Initialize(1);
	nvg2->Initialize(2);


	

	MATRIX data = kpls->GetDataPtr(num_rows,num_cols);
	VECTOR ids = kpls->GetIDPtr();
	VECTOR training = kpls->GetTrainingPtr();


	int t = 7;
	
	for(int r = 0; r< num_rows; r++)
	{
		if(r < t)
		{
			data[r][0] = 1.0*rand()/RAND_MAX+2.0;
			ids[r] = r+1;
			training[r] = 1;
		}
		else if(r < 2*t)
		{
			data[r][0] = 10.0*rand()/RAND_MAX+3.0;
			ids[r] = r+1;
			training[r] = 2;
		}
		else if (r < (num_rows-2*t)/2+2*t)
		{
			data[r][0] = 1.0*rand()/RAND_MAX+2.0;
			ids[r] = r+1;
			training[r] = -1;
		}
		else
		{
			data[r][0] = 10.0*rand()/RAND_MAX+3.0;
			ids[r] = r+1;
			training[r] = -1;
		}
	}
	kpls->InitVariables();
	kpls->ScaleData();
	kpls->Train();
	kpls->Classify();
	
	VECTOR predictions = kpls->GetPredictions();
	int cor = 0,ncor = 0;
	for(int r = 2*t; r < num_rows; r++)
	{
		if(r < (num_rows-2*t)/2+2*t)
		{
			if(predictions[r] == 1)
				cor++;
			else
				ncor++;
		}
		else
		{
			if(predictions[r] == 2)
				cor++;
			else 
				ncor++;
		}
	}
	printf("It got cor = %d, ncor =  %d\n",cor, ncor);
	scanf("%*d");
	exit(0);
}

int main()//int argc, char **argv)
{
	//ST();
    testKPLS();
	int num_tc = 5;
#define BASE "C:\\Users\\arun\\Research\\Tracking\\harvard\\cache\\forvisit_TSeries-02102009-1455-624\\"
	int argc = num_tc*3+1;

	char ** argv = new char* [argc];
	int ch = 4;
	for(int counter=1; counter <argc; counter++)
	{
		argv[counter] = new char [1024];
	}
	for(int counter =1; counter<=num_tc; counter++)
	{
		sprintf(argv[counter],BASE"smoothed_TSeries-02102009-1455-624_Cycle%03d_CurrentSettings_Ch%d.tif",counter,ch);
	}
	for(int counter =1; counter<=num_tc; counter++)
	{
		sprintf(argv[counter+num_tc],BASE"labeled_TSeries-02102009-1455-624_Cycle%03d_CurrentSettings_Ch%d.tif",counter,ch);
	}
	for(int counter =1; counter<=num_tc; counter++)
	{
		sprintf(argv[counter+num_tc*2],BASE"labeled_tracks_TSeries-02102009-1455-624_Cycle%03d_CurrentSettings_Ch%d.tif",counter,ch);
	}

	printf("Started\n");
	/*int num_files = atoi(argv[1]);
	  int c;*/
	//int counter = 0;
	printf(" I got argc = %d\n",argc);
	int num_t = (argc-1)/3;
	/*
	   char file_name[100][1024];
	   char str[1024];
	   int num_c= atoi(argv[5]);
	   printf("num_files,num_t:%d %d\n",num_files,num_t);
	   */

	/*	
		FILE * fp = fopen(argv[4],"r");
		printf("filename:%s\n",argv[4]);
		c = atoi(argv[2]);*/
	//check num_files

	/*for(int i=0;i<(3*num_files);i++)
	  {
	  fgets(file_name[i],1024,fp);
	  file_name[i][strlen(file_name[i])-1]= '\0';
	  printf("files:%s\n",file_name[i]);
	  if(feof(fp))
	  break;
	  }
	  */


	//LabelImageType::Pointer segmented[MAX_TIME][MAX_TAGS]; // FIXME
	//InputImageType::Pointer images[MAX_TIME][MAX_TAGS];
	int c=1;



	//Now track the cells alone and compute associations

	std::vector<std::vector<FeaturesType> > fvector;
	CellTracker::VVL limages;
	//FeaturesType *farray;

	LabelImageType::Pointer labeled;
	bool fexists = true;
	for(int counter=0; counter<num_t; counter++)
	{
		if(!file_exists(argv[counter+1+2*num_t]))
		{
			fexists = false;
			break;
		}
	}
	if(1)//!fexists)
	{
		CellTracker::VVL limages;
		CellTracker::VVR rimages;
		LabelImageType::Pointer tempsegmented;
		InputImageType::Pointer tempimage;
		std::vector<Color2DImageType::Pointer> input,output;
		char *filename_number = "C:\\Users\\arun\\Research\\Tracking\\harvard\\numbers.bmp";
		Color2DImageType::Pointer number = readImage<Color2DImageType>(filename_number);
		for(int t =0; t<num_t; t++)
		{

			tempimage = readImage<InputImageType>(argv[t+1]);	
			tempsegmented = readImage<LabelImageType>(argv[(t+1)+num_t]);
			tempsegmented = getLargeLabels(tempsegmented,100);
			Color2DImageType::Pointer cimp = getColor2DImage(tempsegmented,2);
			std::vector<FeaturesType> f;
			std::vector<LabelImageType::Pointer> li;
			std::vector<InputImageType::Pointer> ri;
			getFeatureVectorsFarsight(tempsegmented,tempimage,f,t,c);
			for(int counter=0; counter < f.size(); counter++)
			{
				li.push_back(extract_label_image(f[counter].num,f[counter].BoundingBox,tempsegmented));
				ri.push_back(extract_raw_image(f[counter].BoundingBox,tempimage));
				annotateImage(number,cimp,f[counter].num,MAX(f[counter].Centroid[0],0),MAX(f[counter].Centroid[1],0));
			}
			input.push_back(cimp);
			fvector.push_back(f);
			limages.push_back(li);
			rimages.push_back(ri);
		}

		FeatureVariances fvar;
		for(int counter=0; counter < FeatureVariances::N; counter++)
		{
			fvar.variances[counter] = std::numeric_limits<float>::max();
		}
		InputImageType::SizeType imsize = tempimage->GetLargestPossibleRegion().GetSize();
		fvar.bounds[0] = 0;
		fvar.bounds[2] = 0;
		fvar.bounds[4] = 0;
		fvar.bounds[1] = imsize[0]-1;
		fvar.bounds[3] = imsize[1]-1;
		fvar.bounds[5] = imsize[2]-1;
		fvar.distVariance = 100;
		fvar.spacing[0] = 0.965;
		fvar.spacing[1] = 0.965;
		fvar.spacing[2] = 4.0;
		fvar.timeVariance = .1;
		fvar.overlapVariance = 500000;
		fvar.variances[FeatureVariances::VOLUME] = 1000000;

		CellTracker ct(fvector,fvar,limages,rimages);
		ct.run();
		ct.writeGraphViz("C:\\Users\\arun\\Research\\Tracking\\harvard\\graphviz_testSeries.dot");
		ct.writeGraphML("C:\\Users\\arun\\Research\\Tracking\\harvard\\graphviz_testSeries.graphml");
		PAUSE;
		for(int t = 0; t< num_t; t++)
		{
			tempsegmented = readImage<LabelImageType>(argv[(t+1)+num_t]);
			LabelImageType::Pointer track = LabelImageType::New();
			track->SetRegions(tempsegmented->GetLargestPossibleRegion());
			track->Allocate();
			track->FillBuffer(0);
			LabelIteratorType liter1(tempsegmented,tempsegmented->GetLargestPossibleRegion());
			LabelIteratorType liter2(track,track->GetLargestPossibleRegion());
			liter1.GoToBegin();
			liter2.GoToBegin();
			std::set<int> s;
			for(;!liter1.IsAtEnd();++liter1,++liter2)
			{
				int val = liter1.Get();
				if(ct.old_to_new[t].count(val)!=0)
				{
					if(ct.old_to_new[t][val] !=0)
						liter2.Set(ct.old_to_new[t][val]);
					else
						s.insert(val);
				}
				else
				{
					s.insert(val);
				}
			}
			Color2DImageType::Pointer cimp = getColor2DImage(track,2);
			std::vector<FeaturesType> f;
			getFeatureVectorsFarsight(track,tempimage,f,t,c);
			for(int counter=0; counter< f.size(); counter++)
			{
				annotateImage(number,cimp,f[counter].num,MAX(f[counter].Centroid[0],0),MAX(f[counter].Centroid[1],0));
			}
			output.push_back(cimp);
			std::cout<<"could not find :" ;
			copy(s.begin(), s.end(), std::ostream_iterator<int>(std::cout, " "));
			writeImage<LabelImageType>(track,argv[(t+1)+2*num_t]);
		}
		ColorImageType::Pointer colin = getColorImageFromColor2DImages(input);
		ColorImageType::Pointer colout = getColorImageFromColor2DImages(output);

		writeImage<ColorImageType>(colin,"C:\\Users\\arun\\Research\\Tracking\\harvard\\debug\\input.tif");
		writeImage<ColorImageType>(colout,"C:\\Users\\arun\\Research\\Tracking\\harvard\\debug\\output.tif");


	}

	//  //for(int c=1; c<=2; c++)
	//	{
	//		int cur_max = 0;
	//		printf("Finished computing the features\n");
	//		unsigned int * track_labels = new unsigned int[MAX_LABEL];
	//		for(unsigned int cind =0; cind< fvector[0][c-1].size(); cind++)
	//		{
	//			track_labels[fvector[0][c-1][cind].num-1] = cind;
	//			printf("track_labels[%d] = %d\n",cind,fvector[0][c-1][cind].num);
	//		}
	//		segmented[0][c-1] = getLabelsMapped(segmented[0][c-1],fvector[0][c-1],track_labels);
	//		cur_max = fvector[0][c-1].size();
	//		delete [] track_labels;

	//		//sprintf(tracks_cache,"%s/%s",CACHE_PREFIX,file_name[(num_files*2)+(c-1)*num_t]);
	//		writeImage<LabelImageType>(segmented[0][c-1],argv[0+1+2*num_t]);

	//		printf("cur_max  = %d\n",cur_max);
	//		//cur_max is the next allowed track number
	//		for(int t = 0; t< num_t-1; t++)
	//		{
	//			vcl_vector<unsigned> indices = getTimeAssociations(fvector[t+1][c-1],fvector[t][c-1]);
	//			printf("fvector@t.size %d fvector@t+1.size %d indices.size %d\n",
	//              (int)fvector[t][c-1].size(), (int)fvector[t+1][c-1].size(),
	//              (int)indices.size());
	//			track_labels = new unsigned int[MAX_LABEL];
	//			for(unsigned int cind = 0; cind<indices.size(); cind++)
	//			{
	//				if(indices[cind] > 100000)
	//				{
	//					track_labels[fvector[t+1][c-1][cind].num-1] = cur_max++;
	//				}
	//				else
	//				{
	//					track_labels[fvector[t+1][c-1][cind].num-1] = fvector[t][c-1][indices[cind]].num-1;
	//				}
	//				//	printf("track_labels[%d] = %u\n",cind, track_labels[cind]);
	//			}

	//			segmented[t+1][c-1] = getLabelsMapped(segmented[t+1][c-1],fvector[t+1][c-1],track_labels);
	//			for(unsigned int cind =0; cind < fvector[t+1][c-1].size(); cind++)
	//			{
	//				printf("fvector[%d].num = %d\n",cind, fvector[t+1][c-1][cind].num);
	//			}
	//			//sprintf(tracks_cache,"%s/%s",CACHE_PREFIX,file_name[(num_files*2)+((c-1)*num_t)+(t+1)]);
	//			writeImage<LabelImageType>(segmented[t+1][c-1],argv[t+1+1+2*num_t]);
	//       //scanf("%*d");
	//			delete [] track_labels;
	//		}
	//		printf("Cur_max %d\n",cur_max);
	//	}
	//}


	//scanf("%*d");
return 0;
}
