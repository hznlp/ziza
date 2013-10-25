#include <iostream>
#include <string>
#include <sstream>
#include "JKArgs.h"
#include "utils.h"
#include "m1.h"
#include "m2.h"
#include "mhmm.h"
using namespace std;
int _debug=0;
int _interpolate=1;
typedef enum {Intersection, Union}CombineMethod;
int _dump=0;
void usage()
{
	cerr<<"ziza -train -data=[input] [-m1=iteration] [-m2=iteration] [-mhmm=iteration] [-smooth=kn|wb]"<<endl
		<<"ziza -prepareCorpus -src=[source text] -tar=[target text] [-weightFile=file] [-word2id=file]"<<endl
		<<"ziza -eval -gold=[goldfile] -test=[testresult]"<<endl
		<<"ziza -recoverCorpus -data=[corpus] -dic=[word2id]" <<endl
		<<"ziza -recoverModel -model=[corpus] -dic=[word2id]" <<endl
		<<"ziza -combineAlign -s2t=align_s2t -t2s=align_t2s"<<endl
		<<"ziza -adjustBase -delta=[int] -input=[alignfile] [-reverse]"<<endl
		<<"ziza -stat -i=input"<<endl
		<<"ziza -convertGizaAlignment -i=input "<<endl;

	exit(1);
}

void convertGizaAlignment(string& curline, vector<pair<int,int> >& align)
{
	align.clear();
	int count=0;
	//NULL ({ 4 }) resumption ({ 1 }) of ({ 2 }) the ({ }) session ({ 3 5 }) 
	for(int count=0;curline.find("})")!=string::npos;count++)
	{
		string tmp=curline.substr(0,curline.find("})")+2);
		curline.erase(0,curline.find("})")+2);
		string al=tmp.substr(tmp.find("({")+2);
		al.erase(al.find("})"));
		//cout<<al<<endl;
		if(count==0)continue;
		stringstream ss(al);
		while(!ss.eof())
		{
			string tmp="";
			ss>>tmp;
			if(tmp=="")break;
			align.push_back(make_pair(atoi(tmp.c_str()),count));
		}
	}
}

void convertGizaAlignment(JKArgs& args)
{
	if(!args.is_set("i"))usage();
	ifstream is(args.value("i").c_str());
	while(!is.eof())
	{
		string curline="";
		getline(is,curline);
		getline(is,curline);
		getline(is,curline);
		vector<pair<int,int> > align;
		convertGizaAlignment(curline,align);
		for(size_t i=0;i<align.size();i++)
			cout<<align[i].first<<"-"<<align[i].second<<" ";
		cout<<endl;
	}
}

int string2int(map<string,int>& word2id, string& wrd)
{
	if(word2id.find(wrd)==word2id.end())
	{
		int id=word2id.size()+1;
		word2id[wrd]=id;
	}
	return word2id[wrd];
}

void adjustBase(JKArgs& args)
{
	//<<"ziza -covertBase -[reduce|increase] -input=[alignfile] "<<endl
	if(!args.is_set("delta")||!args.is_set("input"))
		usage();
	int delta=atoi(args.value("delta").c_str());
	bool reverse=false;
	if(args.is_set("reverse"))reverse=true;
	ifstream is(args.value("input").c_str());
	while(is.good())
	{
		string curline="";
		getline(is,curline);
		if(is.eof())break;
		//cerr<<"curline: "<<curline<<endl;
		stringstream ss(curline);
		int count=0;
		while(ss.good())
		{
			string tmp="";
			int a,b;
			ss>>tmp;
			if(tmp.find("-")==string::npos)break;
		
			sscanf(tmp.c_str(),"%d-%d",&a,&b);
			a=a+delta;
			b=b+delta;
			if(reverse)
			{
				int tempint=a;
				a=b;
				b=tempint;
			}
			cout<<a<<"-"<<b<<" ";
		}
		cout<<endl;
	}
}

void eval(JKArgs& args)
{
	if(!args.is_set("gold")||!args.is_set("test"))usage();
	eval(args.value("gold"),args.value("test"));
}

void prepareCorpus(JKArgs& args)
{
	if(!args.is_set("src")||!args.is_set("tar"))
		usage();
	ifstream fsrc,ftar,fweight;
	bool uniformWeight=true;
	string word2idFilename="word2id";
	if(args.is_set("word2id")) 
		word2idFilename=args.value("word2id");
	ofstream os(word2idFilename.c_str());
	
	fsrc.open(args.value("src").c_str());
	ftar.open(args.value("tar").c_str());
	if(args.is_set("weightFile")) 
	{
		uniformWeight=false;
		fweight.open(args.value("weightFile").c_str());
	}
	map<string,int> word2id_src,word2id_tar;
	
	while(!fsrc.eof()&&!ftar.eof())
	{
		double weight=1;
		string srcline="",tarline="",weightline="";
		getline(fsrc,srcline);
		getline(ftar,tarline);
		if(!uniformWeight) fweight>>weight;
		
		if(srcline!=""&&tarline!="")
		{
			vector<string> srcwords,tarwords;
			stringToVector(srcline,srcwords);
			stringToVector(tarline,tarwords);
			cout<<weight<<endl;
			for(size_t i=0;i<srcwords.size();i++)
			{
				cout<<string2int(word2id_src,srcwords[i])<<" ";
			}
			cout<<endl;
			for(size_t i=0;i<tarwords.size();i++)
			{
				cout<<string2int(word2id_tar,tarwords[i])<<" ";
			}
			cout<<endl;
		}
		else break;
	}
	os<<"src word2id"<<endl;
	os<<"================"<<endl;
	for(map<string,int>::iterator iter=word2id_src.begin();iter!=word2id_src.end();iter++)
		os<<iter->first<<" "<<iter->second<<endl;
	os<<"\ntar word2id"<<endl;
	os<<"================"<<endl;
	for(map<string,int>::iterator iter=word2id_tar.begin();iter!=word2id_tar.end();iter++)
		os<<iter->first<<" "<<iter->second<<endl;

}

void train(JKArgs& args)
{
	if(!args.is_set("data"))
		usage();
	string resultFilename="align";
	if(args.is_set("interpolate"))_interpolate=atoi(args.value("interpolate").c_str());
	if(args.is_set("result"))resultFilename=args.value("result");
	int m1Iter=0,m2Iter=0,mhmmIter=0;
	bool knsmothing=false;
	if(args.is_set("m1"))
		m1Iter=atoi(args.value("m1").c_str());
	if(args.is_set("m2"))
		m2Iter=atoi(args.value("m2").c_str());
	if(args.is_set("mhmm"))
		mhmmIter=atoi(args.value("mhmm").c_str());

	if(m1Iter>0&&m2Iter==0&&mhmmIter==0)
	{
		M1 m1;
		m1.em(args.value("data"),m1Iter);
		m1.align(args.value("data"),resultFilename+".m1");
	}
	if(m2Iter>0&&mhmmIter==0)
	{
		M2 m2;
		m2.em(args.value("data"),m2Iter);
		m2.align(args.value("data"),resultFilename+".m2");
	}
	if(mhmmIter>0)
	{	
		M1 m1;
		m1.em(args.value("data"),m1Iter);
		m1.align(args.value("data"),resultFilename+".m1");
		
		M2 m2;
		m2.init_w2w(m1.w2w());
		m2.em(args.value("data"),m2Iter);
		m2.align(args.value("data"),resultFilename+".m2");
		
		MHmm mhmm;
		if(args.is_set("smooth"))
		{
			if(args.value("smooth")=="kn")
				mhmm.setSmooth(KN);
			else if(args.value("smooth")=="wb")
				mhmm.setSmooth(WB);
		}
		mhmm.init_w2w(m2.w2w());
		mhmm.em(args.value("data"),mhmmIter);
		mhmm.align(args.value("data"),resultFilename+".mhmm");
	}
}

void recoverCorpus(JKArgs& args)
{
	if(!args.is_set("data")||!args.is_set("dic"))
		usage();
	string dataFilename=args.value("data");
	string dicFilename=args.value("dic");
	map<int,string> id2word_src,id2word_tar;
	ifstream is(dataFilename.c_str()),fdic(dicFilename.c_str());
	ofstream osrc("src"),otar("tar");
	string curline="";
	getline(fdic,curline);
	getline(fdic,curline);
	while(!fdic.eof())
	{
		string curline="";
		getline(fdic,curline);
		if(curline=="")break;
		int id=atoi(curline.substr(curline.find(" ")+1).c_str());
		id2word_src[id]=curline.substr(0,curline.find(" "));
	}

	while(!fdic.eof())
	{
		string curline="";
		getline(fdic,curline);
		if(curline=="")break;
		int id=atoi(curline.substr(curline.find(" ")+1).c_str());
		id2word_tar[id]=curline.substr(0,curline.find(" "));
	}

	while(!is.eof())
	{
		string weightline="",srcline="",tarline="";
		getline(is,weightline);
		getline(is,srcline);
		getline(is,tarline);
		if(srcline==""||tarline==""||weightline=="")break;
		double weight=atof(weightline.c_str());
		vector<string> src,tar;
			
		vector<string> tempwords;
		stringToVector(srcline,tempwords);
			
		for(size_t i=0;i<tempwords.size();i++)
			osrc<<id2word_src[atoi(tempwords[i].c_str())]<<" ";
		osrc<<endl;
			
		stringToVector(tarline,tempwords);
		for(size_t i=0;i<tempwords.size();i++)
			otar<<id2word_tar[atoi(tempwords[i].c_str())]<<" ";
		otar<<endl;
	}
	is.eof();
	osrc.close();
	otar.close();
}

void recoverModel(JKArgs& args)
{
	if(!args.is_set("model")||!args.is_set("dic"))
		usage();
	string modelFilename=args.value("model");
	string dicFilename=args.value("dic");
	
	map<int,string> id2word_src,id2word_tar;
	ifstream is(modelFilename.c_str()),fdic(dicFilename.c_str());
	
	string curline="";
	getline(fdic,curline);
	getline(fdic,curline);
	while(!fdic.eof())
	{
		string curline="";
		getline(fdic,curline);
		if(curline=="")break;
		int id=atoi(curline.substr(curline.find(" ")+1).c_str());
		id2word_src[id]=curline.substr(0,curline.find(" "));
	}

	while(!fdic.eof())
	{
		string curline="";
		getline(fdic,curline);
		if(curline=="")break;
		int id=atoi(curline.substr(curline.find(" ")+1).c_str());
		id2word_tar[id]=curline.substr(0,curline.find(" "));
	}
	id2word_src[0]="<NULL>";
	id2word_tar[0]="<NULL>";

	while(!is.eof())
	{
		string curline="";
		getline(is,curline);
		if(curline=="")break;
		vector<string> tempwords;
		stringToVector(curline,tempwords);
		int srcId=atoi(tempwords[0].c_str());
		int tarId=atoi(tempwords[1].c_str());
		cout<<id2word_src[srcId]<<" "<<id2word_tar[tarId]<<" "<<tempwords[2]<<endl;
	}
	is.eof();
}

void line2alignPairs(string& line, vector<pair<int,int> >& aligns, bool reversed)
{
	aligns.clear();
	vector<string> words;
	stringToVector(line,words);
	for(size_t i=0;i<words.size();i++)
	{
		string wrd=words[i];
		int srcInd=atoi(wrd.substr(0,wrd.find("-")).c_str());
		int tarInd=atoi(wrd.substr(wrd.find("-")+1).c_str());
		if(reversed)
		{
			int temp=srcInd;
			srcInd=tarInd;
			tarInd=temp;
		}
		aligns.push_back(make_pair(srcInd,tarInd));
	}
}

void Intersect(vector<pair<int,int> >& s2tAligns, vector<pair<int,int> >& t2sAligns, vector<pair<int,int> >& result)
{
	map<pair<int,int>, int > alignMap;
	for(size_t i=0;i<s2tAligns.size();i++)
		alignMap[s2tAligns[i]]=1;
	for(size_t i=0;i<t2sAligns.size();i++)
		if(alignMap.find(t2sAligns[i])!=alignMap.end())
			result.push_back(t2sAligns[i]);
}

void United(vector<pair<int,int> >& s2tAligns, vector<pair<int,int> >& t2sAligns, vector<pair<int,int> >& result)
{
	result=s2tAligns;
	map<pair<int,int>, int > alignMap;
	for(size_t i=0;i<s2tAligns.size();i++)
		alignMap[s2tAligns[i]]=1;
	for(size_t i=0;i<t2sAligns.size();i++)
		if(alignMap.find(t2sAligns[i])==alignMap.end())
			result.push_back(t2sAligns[i]);
}

bool isNeighbour(pair<int,int>& bead1, pair<int,int>& bead2)
{
	if(abs(bead1.first-bead2.first)<2&&abs(bead1.second-bead2.second)<2)
		return true;
	else return false;
}

void grow_diag(vector<pair<int,int> >& intersect, vector<pair<int,int> >& united, map<pair<int,int>, int >& alignMap)
{
	map<pair<int,int>, int > leftMap;
	for(size_t i=0;i<intersect.size();i++)
		alignMap[intersect[i]]=1;
	for(int i=united.size()-1;i>=0;i--)
		if(alignMap.find(united[i])==alignMap.end())
			leftMap[united[i]]=1;

	int newAdded=1;
	while(newAdded!=0)
	{
		newAdded=0;
		map<pair<int,int>,int> selected;
		for(map< pair<int,int> , int>::iterator aIter=alignMap.begin();aIter!=alignMap.end();aIter++)
		{
			pair<int,int> bead=aIter->first; 
			for(map< pair<int,int> , int>::iterator lIter=leftMap.begin();lIter!=leftMap.end();lIter++)
			{
				pair<int,int> leftBead=lIter->first;
				if(isNeighbour(bead,leftBead))
					selected[leftBead]=1;
			}
		}
		for(map< pair<int,int> , int>::iterator sIter=selected.begin();sIter!=selected.end();sIter++)
		{
			alignMap[sIter->first]=1;
			leftMap.erase(leftMap.find(sIter->first));
		}
		newAdded=selected.size();
		selected.clear();
	}
}

void combine(vector<pair<int,int> >& s2tAligns, vector<pair<int,int> >& t2sAligns, vector<pair<int,int> >& result, CombineMethod combineMethod)
{
	result.clear();
	vector<pair<int,int> > intersect,united;
	map<pair<int,int> , int> alignMap;
	Intersect(s2tAligns,t2sAligns,intersect);
	United(s2tAligns,t2sAligns,united);
	grow_diag(intersect,united,alignMap);
	for(map< pair<int,int> , int>::iterator aIter=alignMap.begin();aIter!=alignMap.end();aIter++)
	{
		pair<int,int> bead=aIter->first;
		result.push_back(bead);
	}
	if(combineMethod==Intersection)
		result=intersect;
	else if(combineMethod==Union)
		result=united;
}

void combineAlign(JKArgs& args)
{
	if(!args.is_set("s2t")||!args.is_set("t2s"))
		usage();
	CombineMethod cm=Intersection;
	if(args.is_set("combineMethod"))
	{
		if(args.value("combineMethod")=="Union")
			cm=Union;
	}
	ifstream fs2t(args.value("s2t").c_str()), ft2s(args.value("t2s").c_str());
	while(!fs2t.eof()&&!ft2s.eof())
	{
		string s2tline="";
		string t2sline="";
		getline(fs2t,s2tline);
		getline(ft2s,t2sline);
		vector<pair<int,int> > s2tAligns,t2sAligns,result;
		line2alignPairs(s2tline,s2tAligns,false);
		line2alignPairs(t2sline,t2sAligns,true);
		combine(s2tAligns,t2sAligns,result,cm);
		for(size_t i=0;i<result.size();i++)
			cout<<result[i].first<<"-"<<result[i].second<<" ";
		cout<<endl;
	}
}

void stat(JKArgs& args)
{
	if(!args.is_set("i"))usage();
	ifstream is(args.value("i").c_str());
	int nline=0,nword=0,ntype=0,nsingletons=0;
	map<string,int> word2count;
	while(!is.eof())
	{
		string curline="";
		getline(is,curline);
		if(curline=="")break;
		nline++;
		vector<string> words;
		stringToVector(curline,words);
		nword+=words.size();
		for(size_t i=0;i<words.size();i++)
			word2count[words[i]]+=1;
	}
	ntype=word2count.size();
	for(map<string,int>::iterator iter=word2count.begin();iter!=word2count.end();iter++)
	{
		if(iter->second==1)
			nsingletons++;
	}
	cerr<<"nline "<<nline<<" , nwords "<<nword<<" , ntype "<<ntype<<" , "<<nsingletons<<endl;
}

int main(int argc, char** argv)
{
	JKArgs args(argc,argv);
	if(args.is_set("debug"))_debug=atoi(args.value("debug").c_str());
        if(args.is_set("dump"))_dump=1;
        if(args.is_set("prepareCorpus"))
	{
		prepareCorpus(args);
	}
	else if(args.is_set("train"))
	{
		train(args);
	}
	else if(args.is_set("eval"))
	{
		eval(args);
	}
	else if(args.is_set("recoverCorpus"))
	{
		recoverCorpus(args);
	}
	else if(args.is_set("recoverModel"))
	{
		recoverModel(args);
	}
	else if(args.is_set("combineAlign"))
	{
		combineAlign(args);
	}
	else if(args.is_set("stat"))
	{
		stat(args);
	}
	else if(args.is_set("adjustBase"))
	{
		adjustBase(args);
	}
	else if(args.is_set("convertGizaAlignment"))
	{
		convertGizaAlignment(args);
	}
	else usage();
	return 1;
}

