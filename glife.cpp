#include "glife.h"

using namespace std;

int gameOfLife(int argc, char *argv[]);
void singleThread(int, int, int);
void* workerThread(void *);
int nprocs;
GameOfLifeGrid* g_GameOfLifeGrid;

uint64_t dtime_usec(uint64_t start)
{
  timeval tv;
  gettimeofday(&tv, 0);
  return ((tv.tv_sec*USECPSEC)+tv.tv_usec)-start;
}

GameOfLifeGrid::GameOfLifeGrid(int rows, int cols, int gen)
{
  m_Generations = gen;
  m_Rows = rows;
  m_Cols = cols;

  m_Grid = (int**)malloc(sizeof(int*) * rows);
  if (m_Grid == NULL) 
    cout << "1 Memory allocation error " << endl;

  m_Temp = (int**)malloc(sizeof(int*) * rows);
  if (m_Temp == NULL) 
    cout << "2 Memory allocation error " << endl;

  m_Grid[0] = (int*)malloc(sizeof(int) * (cols*rows));
  if (m_Grid[0] == NULL) 
    cout << "3 Memory allocation error " << endl;

  m_Temp[0] = (int*)malloc(sizeof(int) * (cols*rows));	
  if (m_Temp[0] == NULL) 
    cout << "4 Memory allocation error " << endl;

  for (int i = 1; i < rows; i++) {
    m_Grid[i] = m_Grid[i-1] + cols;
    m_Temp[i] = m_Temp[i-1] + cols;
  }

  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      m_Grid[i][j] = m_Temp[i][j] = 0;
    }
  }
}

// Entry point
int main(int argc, char* argv[])
{
  if (argc != 7) {
    cout <<"Usage: " << argv[0] << " <input file> <display> <nprocs>"
           " <# of generation> <width> <height>" << endl;
    cout <<"\n\tnprocs = 0: Running sequentiallyU" << endl;
    cout <<"\tnprocs =1: Running on a single thread" << endl;
    cout <<"\tnprocs >1: Running on multiple threads" << endl;
    cout <<"\tdisplay = 1: Dump results" << endl;
    return 1;
  }

  return gameOfLife(argc, argv);
}

int gameOfLife(int argc, char* argv[])
{
  int cols, rows, gen;
  ifstream inputFile;
  int input_row, input_col, display;
  uint64_t difft;
  pthread_t *threadID;

  inputFile.open(argv[1], ifstream::in);

  if (inputFile.is_open() == false) {
    cout << "The "<< argv[1] << " file can not be opend" << endl;
    return 1;
  }

  display = atoi(argv[2]);
  nprocs = atoi(argv[3]);
  gen = atoi(argv[4]);
  cols = atoi(argv[5]);
  rows = atoi(argv[6]);

  g_GameOfLifeGrid = new GameOfLifeGrid(rows, cols, gen);

  while (inputFile.good()) {
    inputFile >> input_row >> input_col;
    if (input_row >= rows || input_col >= cols) {
      cout << "Invalid grid number" << endl;
      return 1;
    } else
      g_GameOfLifeGrid->setCell(input_row, input_col);
  }

  // Start measuring execution time
  difft = dtime_usec(0);

  // TODO: YOU NEED TO IMPLMENT THE SINGLE THREAD, PTHREAD, AND CUDA
  if (nprocs == 0) {
    // Running with your sequential version
    while(g_GameOfLifeGrid->decGen()) {
      g_GameOfLifeGrid->next();
    }
  } else if (nprocs == 1) {
    // Running a single thread
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, nprocs);

    MultipleArg multiple_arg = { 0, rows, gen, &barrier };
    pthread_t thread;
    pthread_create(&thread, NULL, workerThread, (void*)&multiple_arg);
    pthread_join(thread, NULL);

    // singleThread(rows, cols, gen);
  } else { 
    // Running multiple threads (pthread)
    int quotient = rows / nprocs;
    int remainder = rows % nprocs;

    int point = 0;
    MultipleArg multiple_arg[nprocs];

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, nprocs);

    for (int i = 0; i < nprocs; i++) {
      multiple_arg[i].start = point;
      if (remainder > 0) {
        remainder--;
        point++;
      }
      point += quotient;
      multiple_arg[i].end = point;
      multiple_arg[i].gen = gen;
      multiple_arg[i].barrier = &barrier;
    }

    pthread_t threads[nprocs];

    for (int i = 0; i < nprocs; i++) {
      pthread_create(&threads[i], NULL, workerThread, (void*)&multiple_arg[i]);
    }

    for (int i = 0; i < nprocs; i++) {
      pthread_join(threads[i], NULL);
    }
  }

  difft = dtime_usec(difft);

  // Print indices only for running on CPU(host).
  if (display) {
    g_GameOfLifeGrid->dump();
    g_GameOfLifeGrid->dumpIndex();
  }

  // Sequential or Single or multi-thread execution time 
  cout << "Execution time(seconds): " << difft/(float)USECPSEC << endl;

  inputFile.close();
  cout << "Program end... " << endl;
  return 0;
}

// TODO: YOU NEED TO IMPLMENT PTHREAD
void* workerThread(void *arg)
{
  MultipleArg *args = (MultipleArg*) arg;
  int temp_gen = args->gen;
  
  while(temp_gen--) {
    g_GameOfLifeGrid->next(args->start, args->end);
    pthread_barrier_wait(args->barrier);

    for (int i = 0; i < g_GameOfLifeGrid->getRows(); i++) {
      for (int j = 0; j < g_GameOfLifeGrid->getCols(); j++) {
        g_GameOfLifeGrid->temp2Grid(i, j);
      }
    }
    pthread_barrier_wait(args->barrier);
  }

  pthread_exit(NULL);
}

// HINT: YOU MAY NEED TO FILL OUT BELOW FUNCTIONS OR CREATE NEW FUNCTIONS
/* multi thread */
void GameOfLifeGrid::next(const int from, const int to)
{
  for (int i = from; i < to; i++) {
    for (int j = 0; j < m_Cols; j++) {
      if (isLive(i, j)) {
        if (getNumOfNeighbors(i, j) == 2 || getNumOfNeighbors(i, j) == 3) live(i, j);
        else dead(i, j);
      }
      else {
        if (getNumOfNeighbors(i, j) == 3) live(i, j);
        else dead(i, j);
      }
    }
  }
}

/* sequence mode */
void GameOfLifeGrid::next()
{
  /* 현재 cell이 다음 generation에 live인지 dead인지 계산 -> temp에 저장 */
  for (int i = 0; i < m_Rows; i++) {
    for (int j = 0; j < m_Cols; j++) {
      if (isLive(i, j)) {
        if (getNumOfNeighbors(i, j) == 2 || getNumOfNeighbors(i, j) == 3) live(i, j);
        else dead(i, j);
      }
      else {
        if (getNumOfNeighbors(i, j) == 3) live(i, j);
        else dead(i, j);
      }
    }
  }

  /* temp에 저장된 값을 m_Grid에 저장 */
  for (int i = 0; i < m_Rows; i++) {
    for (int j = 0; j < m_Cols; j++) {
      temp2Grid(i, j);
    }
  }
}

// TODO: YOU MAY NEED TO IMPLMENT IT TO GET NUMBER OF NEIGHBORS 
int GameOfLifeGrid::getNumOfNeighbors(int rows, int cols)
{
  /* 현재 neighbor가 몇 개 있는지 계산 */
  int numOfNeighbors = 0;
  for(int i = rows - 1; i <= rows + 1; i++) {
    for (int j = cols - 1; j <= cols + 1; j++) {
      if ((i == rows && j == cols) || i < 0 || i >= m_Rows || j < 0 || j >= m_Cols)
        continue;
      if (isLive(i, j))
        numOfNeighbors++;
    }
  }
  return numOfNeighbors;
}

void GameOfLifeGrid::dump() 
{
  cout << "===============================" << endl;

  for (int i = 0; i < m_Rows; i++) {
    cout << "[" << i << "] ";
    for (int j = 0; j < m_Cols; j++) {
      if (m_Grid[i][j] == 1)
        cout << "*";
      else
        cout << "o";
    }
    cout << endl;
  }
  cout << "===============================\n" << endl;
}

void GameOfLifeGrid::dumpIndex()
{
  cout << ":: Dump Row Column indices" << endl;
  for (int i=0; i < m_Rows; i++) {
    for (int j=0; j < m_Cols; j++) {
      if (m_Grid[i][j]) cout << i << " " << j << endl;
    }
  }
}
