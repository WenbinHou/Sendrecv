#include <nonblocking/rdma_comm.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
int myrank;
int allsize;
std::vector<nodeinfo> nodelist;

void samplesort(double *locey, int n)
{
    double *splitter=NULL, *allsplitter_data=NULL, *allsplitters=NULL;
    double *mybucket=NULL;
    //前nprocs+1个元素记录被nprocs-1个分割点和2个起止点，后nprocs个元素记录的是nprocs个区间的个数
    //每个分割点属于左边的分割区间，即区间是呈现类似左开右闭形式
    splitter = (double*)malloc((2*nprocs+1)*sizeof(double));
    int tmppre = -1;
    splitter[0] = locey[0];//cout << splitter[0] << " ";
    int index = nprocs;
    int sum = 0;
    for(int i = 1;i <= nprocs;i++){
        int tmplast = i*n/nprocs-1;
        splitter[i] = locey[tmplast];
        splitter[index+i] = tmplast-tmppre;
        sum += (tmplast-tmppre);
        tmppre = tmplast;
    }
    //将splitter数组发送到0号进程
    if(myid == 0)
    {
        allsplitter_data = (double*)malloc(nprocs*(2*nprocs+1)*sizeof(double));
    }
    MPI_Gather(splitter, 2*nprocs+1, MPI_DOUBLE, allsplitter_data, 2*nprocs+1, MPI_DOUBLE, Root, MPI_COMM_WORLD);
    double* totalsplitter = (double*)malloc(sizeof(double)*(2*nprocs-1));
    if(myid==Root)
    {
        allsplitters=(double*)malloc(sizeof(double)*nprocs*(nprocs-1));
        double *tmpsplitters = allsplitters;int s = 1;
        for(int i = 0;i < nprocs;i++)
        {
            memcpy(tmpsplitters, allsplitter_data+s, sizeof(double)*(nprocs-1));
            s += (2*nprocs+1);tmpsplitters += (nprocs-1);
        }
        sort(allsplitters, allsplitters+nprocs*(nprocs-1));
        //再次选择切分点
        for(int i=0;i < nprocs-1;i++) totalsplitter[i]=allsplitters[(nprocs-1)*(i+1)];
        double *tmpblocknum = (double*)malloc(sizeof(double)*nprocs);
        memset(tmpblocknum, 0, sizeof(double)*nprocs);
        //遍历allsplitter_data来确定每一个进程至多的个数
        for(int i = 0;i < nprocs;i++)
        {
            int splitindex = 0, t = i*(2*nprocs+1);//最大是nprocs-2
            double splitnum = totalsplitter[splitindex];
            int j = 0, k = 0;
            while(t < (i+1)*(2*nprocs+1)-nprocs-1 && k < nprocs-1)
            {
                double start=allsplitter_data[t], end=allsplitter_data[t+1];
                if(start <= splitnum)
                {
                    tmpblocknum[k] += allsplitter_data[i*(2*nprocs+1)+nprocs+1+j];
                    if(end <= splitnum){j++;t++;}
                    else {k++;splitnum=totalsplitter[++splitindex];}
                }
                else
                {
                    splitnum=totalsplitter[++splitindex];
                    k++;
                }
            }
            while(j < nprocs)
            {
                tmpblocknum[k] += allsplitter_data[i*(2*nprocs+1)+nprocs+1+j];j++;
            }
        }
        memcpy(totalsplitter+nprocs-1, tmpblocknum, sizeof(double)*nprocs);
        if(tmpblocknum) free(tmpblocknum);
    }
    MPI_Bcast(totalsplitter, 2*nprocs-1, MPI_DOUBLE, Root, MPI_COMM_WORLD);
    //根据接收到的数据，开辟足够大小的空间
    mybucket = (double*)malloc(sizeof(double)*(totalsplitter[nprocs-1+myid]));
    //此处应该有个同步
    MPI_Barrier(MPI_COMM_WORLD);
    int *sendcounts = (int*)malloc(sizeof(int)*nprocs);memset(sendcounts, 0, nprocs*sizeof(int));
    for(int i=0,j=0;i < nprocs-1;i++){
        for(;j < n;j++){
            if(locey[j]<=totalsplitter[i])sendcounts[i]++;
            else break;
        }
    }
    sendcounts[nprocs-1]=n;
    for(int i=0;i < nprocs-1;i++)sendcounts[nprocs-1] -= sendcounts[i];
    //使用Isend和recv进行发送
    int maxrecvnum = totalsplitter[nprocs-1+myid];int ss=0;double *mybuckettmp=mybucket;
    double *loceytmp = locey;
    for(int i=0;i < nprocs;i++)
    {
        MPI_Request req;MPI_Status status;
        MPI_Isend(loceytmp, sendcounts[i], MPI_DOUBLE, i, myid, MPI_COMM_WORLD, &req);
        MPI_Recv(mybuckettmp, maxrecvnum, MPI_DOUBLE, i, i, MPI_COMM_WORLD, &status);
        loceytmp += sendcounts[i];
        int realrecvnum;
        MPI_Get_count(&status, MPI_DOUBLE, &realrecvnum);
        maxrecvnum -= realrecvnum;
        mybuckettmp += realrecvnum;
        ss += realrecvnum;
    }
    sort(mybucket, mybucket+ss);
    MPI_Request r;
    MPI_Isend(mybucket, ss, MPI_DOUBLE, Root, myid, MPI_COMM_WORLD, &r);
    if(myid == 0)
    {
        double maxsize = 0;
        for(int i=0;i < nprocs;i++)
            maxsize = max(totalsplitter[nprocs-1+myid], maxsize);
        double *outputbuf = (double*)malloc(sizeof(double)*(int)maxsize);
        FILE *fp=fopen("sort.out", "w");
        for(int i = 0;i < nprocs;i++)
        {
            MPI_Status status;
            MPI_Recv(outputbuf, maxsize, MPI_DOUBLE, i, i, MPI_COMM_WORLD, &status);
            int realrecvnum;
            MPI_Get_count(&status, MPI_DOUBLE, &realrecvnum);
            for(int j=0;j <realrecvnum;j++)fprintf(fp, "%lf\n", outputbuf[j]);
        }fclose(fp);
    }
    //清理开辟的内存
    if(splitter)free(splitter);
    if(allsplitter_data)free(allsplitter_data);
    if(allsplitters)free(allsplitters);
    if(totalsplitter)free(totalsplitter);
    if(sendcounts)free(sendcounts);
    if(mybucket)free(mybucket);
}

void read_host_file(char* file)
{
    std::ifstream fin(file, std::ios::in);
    char line[1024]={0};
    while(fin.getline(line, sizeof(line)))
    {
        std::string lis_ip; int lis_port;
        std::stringstream word(line);
        word >> lis_ip;
        word >> lis_port;
        nodelist.emplace_back(lis_ip, lis_port);
    }
    fin.close();
    int i = 0;
    for(auto &node:nodelist){
        DEBUG("rank:%d ip_addr:%s port:%d\n", i, nodelist[i].ip_addr, nodelist[i].listen_port);
        i++;
    }
    allsize = nodelist.size();
}

int main(int argc, char *argv[])
{
    char nodelist_file_path[256];
    if(argc < 5){
        ERROR("error parameter only %d parameters.\n", argc);
        exit(0);
    }
    int op;
    while ((op = getopt(argc, argv, "i:f:n:")) != -1){
        switch(op){
            case 'i':
                myrank = atoi(optarg);
                break;
            case 'f':
                strcpy(nodelist_file_path, optarg);
                ITRACE("nodelist_file_path is [%s]\n", nodelist_file_path);
                read_host_file(nodelist_file_path);
                break;
            default:
                ERROR("parameter is error.\n");
                return 0;
        }
    }

    rdma_comm comm_object;
    comm_object.init(myrank, allsize, nodelist);
    srand((unsigned)myrank);
    int n = rand()%51+50;
    double *array = (double*)malloc(n * sizeof(double));
    for(int i = 0;i < n;i++){
        array[i] = rand()/(double)(RAND_MAX/100);
    }
    samplesort(array, n);

    /*MPI_Init(&argc, &argv); MPI_Comm_size(MPI_COMM_WORLD, &nprocs); MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    srand((unsigned)myid);
    //srand(0xdeadbeef);
    //随机产生范围在50-100的一个整数
    int n = rand()%51+50;
    double *locey = (double*)malloc(n* sizeof(double));
    for(int i=0;i<n;i++){locey[i]=rand()/(double)(RAND_MAX/100);}
    sort(locey, locey+n);
    samplesort(locey, n);
    MPI_Finalize();*/
    return 0;
}
