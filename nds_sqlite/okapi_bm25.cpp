
#include "okapi_bm25.h"
#include <math.h>

//SQLITE_EXTENSION_INIT1

static void okapi_bm25(sqlite3_context *pCtx, int nVal, sqlite3_value **apVal)
{
    assert(sizeof(int) == 4);

    unsigned int *matchinfo = (unsigned int *)sqlite3_value_blob(apVal[0]);
    int searchTextCol = sqlite3_value_int(apVal[1]);

    double K1 = ((nVal >= 3) ? sqlite3_value_double(apVal[2]) : 1.2);
    double B = ((nVal >= 4) ? sqlite3_value_double(apVal[3]) : 0.75);

    int P_OFFSET = 0;
    int C_OFFSET = 1;
    int X_OFFSET = 2;
    
    int termCount = matchinfo[P_OFFSET];
    int colCount = matchinfo[C_OFFSET];
    
    int N_OFFSET = X_OFFSET + 3*termCount*colCount;
    int A_OFFSET = N_OFFSET + 1;
    int L_OFFSET = (A_OFFSET + colCount);


    double totalDocs = matchinfo[N_OFFSET];
    double avgLength = matchinfo[A_OFFSET + searchTextCol];
    double docLength = matchinfo[L_OFFSET + searchTextCol];

    double sum = 0.0;

    for (int i = 0; i < termCount; i++) {
//        int currentX = X_OFFSET + (3 * searchTextCol * (i + 1));
      int currentX = X_OFFSET + (3 * (searchTextCol + (i * colCount)));
      double termFrequency = matchinfo[currentX];
      double docsWithTerm = matchinfo[currentX + 2];
      
      double idf = log(
        (totalDocs - docsWithTerm + 0.5) /
        (docsWithTerm + 0.5)
      );

      double rightSide = (
        (termFrequency * (K1 + 1)) /
        (termFrequency + (K1 * (1 - B + (B * (docLength / avgLength)))))
      );

      sum += (idf * rightSide);
    }

    sqlite3_result_double(pCtx, sum);
}

static void okapi_bm25f(sqlite3_context *pCtx, int nVal, sqlite3_value **apVal) {
    assert(sizeof(int) == 4);
    
    unsigned int *matchinfo = (unsigned int *)sqlite3_value_blob(apVal[0]);
   
    double K1 = 1.2;// ((nVal >= 3) ? sqlite3_value_double(apVal[2]) : 1.2);
    double B = 0.75;// ((nVal >= 4) ? sqlite3_value_double(apVal[3]) : 0.75);
    
    int P_OFFSET = 0;
    int C_OFFSET = 1;
    int X_OFFSET = 2;
    
    int termCount = matchinfo[P_OFFSET];
    int colCount = matchinfo[C_OFFSET];
    
    int N_OFFSET = X_OFFSET + 3*termCount*colCount;
    int A_OFFSET = N_OFFSET + 1;
    int L_OFFSET = (A_OFFSET + colCount);
//    int S_OFFSET = (L_OFFSET + colCount); //useful as a pseudo proximity weighting per field/column
    
    double totalDocs = matchinfo[N_OFFSET];
    
    double avgLength = 0.0;
    double docLength = 0.0;
    
    for (int col = 0; col < colCount; col++)
    {
        avgLength +=  matchinfo[A_OFFSET + col];
        docLength +=  matchinfo[L_OFFSET + col];
    }
    
    double epsilon = 1.0 / (totalDocs*avgLength);
    double sum = 0.0;
    
    for (int t = 0; t < termCount; t++) {
        for (int col = 0 ; col < colCount; col++)
        {
            //int currentX = X_OFFSET + (3 * col * (t + colCount));
            int currentX = X_OFFSET + (3 * (col + (t * colCount)));
            
            double termFrequency = matchinfo[currentX];
            double docsWithTerm = matchinfo[currentX + 2];
            
            double idf = log(
                             (totalDocs - docsWithTerm + 0.5) /
                             (docsWithTerm + 0.5)
                             );
            
            idf = (idf < 0) ? epsilon : idf; //common terms could have no effect (\epsilon=0.0) or a very small effect (\epsilon=1/NoOfTokens which asymptotes to 0.0)

            double rightSide = (
                                (termFrequency * (K1 + 1)) /
                                (termFrequency + (K1 * (1 - B + (B * (docLength / avgLength)))))
                                );
            
            rightSide += 1.0;

            
            double weight = ((nVal > col+1) ? sqlite3_value_double(apVal[col+1]) : 1.0);
            
//            double subsequence = matchinfo[S_OFFSET + col];
            
            sum += (idf * rightSide) * weight; // * subsequence; //useful as a pseudo proximty weighting
        }
    }
    
    sqlite3_result_double(pCtx, sum);
}

static void okapi_bm25f_kb(sqlite3_context *pCtx, int nVal, sqlite3_value **apVal) {
    assert(sizeof(int) == 4);
    
    unsigned int *matchinfo = (unsigned int *)sqlite3_value_blob(apVal[0]);
    
    
    //Setting the default values and ignoring argument based inputs so the extra
    //arguments can be the column weights instead.
    if (nVal < 2) sqlite3_result_error(pCtx, "wrong number of arguments to function okapi_bm25_kb(), expected k1 parameter", -1);
    if (nVal < 3) sqlite3_result_error(pCtx, "wrong number of arguments to function okapi_bm25_kb(), expected b parameter", -1);
    double K1 = sqlite3_value_double(apVal[1]);
    double B = sqlite3_value_double(apVal[2]);
    
    int P_OFFSET = 0;
    int C_OFFSET = 1;
    int X_OFFSET = 2;
    
    int termCount = matchinfo[P_OFFSET];
    int colCount = matchinfo[C_OFFSET];
    
    int N_OFFSET = X_OFFSET + 3*termCount*colCount;
    int A_OFFSET = N_OFFSET + 1;
    int L_OFFSET = (A_OFFSET + colCount);
    //    int S_OFFSET = (L_OFFSET + colCount); //useful as a pseudo proximity weighting per field/column
    
    double totalDocs = matchinfo[N_OFFSET];
    
    double avgLength = 0.0;
    double docLength = 0.0;
    
    for (int col = 0; col < colCount; col++)
    {
        avgLength +=  matchinfo[A_OFFSET + col];
        docLength +=  matchinfo[L_OFFSET + col];
    }
    
    double epsilon = 1.0 / (totalDocs*avgLength);
    double sum = 0.0;
    
    for (int t = 0; t < termCount; t++) {
        for (int col = 0 ; col < colCount; col++)
        {
//            int currentX = X_OFFSET + (3 * col * (t + 1));
            int currentX = X_OFFSET + (3 * (col + (t * colCount)));
            
            double termFrequency = matchinfo[currentX];
            double docsWithTerm = matchinfo[currentX + 2];
            
            double idf = log(
                             (totalDocs - docsWithTerm + 0.5) /
                             (docsWithTerm + 0.5)
                             );
            
            idf = (idf < 0) ? epsilon : idf; //common terms could have no effect (\epsilon=0.0) or a very small effect (\epsilon=1/NoOfTokens which asymptotes to 0.0)
            
            double rightSide = (
                                (termFrequency * (K1 + 1)) /
                                (termFrequency + (K1 * (1 - B + (B * (docLength / avgLength)))))
                                );
            
            rightSide += 1.0;
            
            double weight = ((nVal > col+3) ? sqlite3_value_double(apVal[col+3]) : 1.0);
            
            //            double subsequence = matchinfo[S_OFFSET + col];
            
            sum += (idf * rightSide) * weight; // * subsequence; //useful as a pseudo proximty weighting
        }
    }
    
    sqlite3_result_double(pCtx, sum);
}

///<  matchinfo(TableName, 'pcyl')
static void mx_rank_function(sqlite3_context *pCtx, int nVal, sqlite3_value **apVal) {

	unsigned int *matchinfo = (unsigned int *)sqlite3_value_blob(apVal[0]);

	if (nVal < 2) sqlite3_result_error(pCtx, "wrong number of arguments to function okapi_bm25_kb(), expected k1 parameter", -1);
	if (nVal < 3) sqlite3_result_error(pCtx, "wrong number of arguments to function okapi_bm25_kb(), expected b parameter", -1);
	double K1 = sqlite3_value_double(apVal[1]);
	double B = sqlite3_value_double(apVal[2]);

	int P_OFFSET = 0;
	int C_OFFSET = 1;
	int Y_OFFSET = 2;

	int termCount = matchinfo[P_OFFSET];
	int colCount = matchinfo[C_OFFSET];

	int L_OFFSET = Y_OFFSET + termCount*colCount;

	double matchcost_0 = 0.0;
	double matchcost_1 = 0.0;

	for (int t = 0; t < termCount; t++) {
		double term_matchcost_max = 0;
		for (int col = 0 ; col < colCount; col++)
		{
			int currentY = Y_OFFSET + (col + (t * colCount));
			int docLength = matchinfo[L_OFFSET + col];
			double term_matchcost_0 = 0;
			double termFrequency = (matchinfo[currentY] == 0) ? 0 : 1;
			double weight = ((nVal > col+3) ? sqlite3_value_double(apVal[col+3]) : 1.0);

			if(docLength == 0)
				continue;
			//matchcost_0 += termFrequency*weight;
			term_matchcost_0 = termFrequency*weight;
			term_matchcost_max = (term_matchcost_0>term_matchcost_max) ? term_matchcost_0 : term_matchcost_max;
			matchcost_1 += termFrequency/docLength*weight;
		}

		matchcost_0 += term_matchcost_max;
	}

	//	double *p = (double *)sqlite3_malloc(sizeof(double)*2);

	double p[2] = {0};
	p[0] = matchcost_0;
	p[1] = matchcost_1;

	sqlite3_result_blob(pCtx, p, sizeof(double)*2, SQLITE_TRANSIENT);
}

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
//    SQLITE_EXTENSION_INIT2(pApi)

    sqlite3_create_function(db, "okapi_bm25", -1, SQLITE_ANY, 0, okapi_bm25, 0, 0);
    sqlite3_create_function(db, "okapi_bm25f", -1, SQLITE_UTF8, 0, okapi_bm25f, 0, 0);
    sqlite3_create_function(db, "okapi_bm25f_kb", -1, SQLITE_UTF8, 0, okapi_bm25f_kb, 0, 0);
    sqlite3_create_function(db, "mx_rank", -1, SQLITE_UTF8, 0, mx_rank_function, 0, 0);

    return 0;
}