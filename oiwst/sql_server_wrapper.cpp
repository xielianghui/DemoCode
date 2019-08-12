#include"sql_server_wrapper.h"

#define BUFSIZE 1024 // freetds 的bug, 自己指定足够的空间 https://stackoverflow.com/questions/6602726/dbisybase-data-conversion-resulted-in-overflow

SqlServerHdl* SqlServerHdl::m_instance = nullptr;

SqlServerHdl::SqlServerHdl():m_dbproc(nullptr){}

SqlServerHdl::~SqlServerHdl()
{
    if(m_dbproc){
        dbclose(m_dbproc);
    }
}

SqlServerHdl* SqlServerHdl::Instance()
{
    if(m_instance == nullptr){
        static std::once_flag flag;
        std::call_once(flag,[](){
            m_instance = new SqlServerHdl();
        });
    }
    return m_instance;
}

int SqlServerHdl::Init(DBConfig& cfg)
{
    // init
    RETCODE ret = dbinit();
    if(ret == FAIL){
        std::cout<<"dbinit() failed"<<std::endl;
        return -1;
    }
    // msg bind
    dbmsghandle(MsgHandler);
    dberrhandle(ErrHandler);
    // login set
    LOGINREC* loginrec = dblogin();
    DBSETLUSER(loginrec, cfg.userName.c_str());// set login user name
    DBSETLPWD(loginrec, cfg.password.c_str()); // set login password
    // connect 
    std::string serverAddr = cfg.host + ":" + cfg.port;
    m_dbproc = dbopen(loginrec, serverAddr.c_str());
    if(m_dbproc == nullptr){
        std::cout<<"dbopen() failed"<<std::endl;
        return -1;
    }
    // use db
    ret = dbuse(m_dbproc, cfg.dbName.c_str());
    if(ret == FAIL){
        std::cout<<"dbuse() failed"<<std::endl;
        return -1;
    }
    return 0;
}

int SqlServerHdl::QueryCmd(const std::string& cmd, std::vector<std::vector<std::string>>& res)
{
    return Cmd(CMDTYPE::SELECT, cmd, res);
}

int SqlServerHdl::InsertCmd(const std::string& cmd)
{
    std::vector<std::vector<std::string>> res;
    return Cmd(CMDTYPE::INSERT, cmd, res);
}

int SqlServerHdl::UpdateCmd(const std::string& cmd)
{
    std::vector<std::vector<std::string>> res;
    return Cmd(CMDTYPE::UPDATE, cmd, res);
}

int SqlServerHdl::Cmd(CMDTYPE type, const std::string& cmd, std::vector<std::vector<std::string>>& res)
{
    if(m_dbproc)
    {
        RETCODE ret = SUCCEED;
        dbcancel(m_dbproc); // clear dataset last time
        int ncols = 0; // col counts
        int rowCode = 0; // after goto can't declare val
        // exec cmd
        dbcmd(m_dbproc, cmd.c_str());
        ret = dbsqlexec(m_dbproc);
        if(ret == FAIL){
            std::cout<<"dbsqlexec() failed"<<std::endl;
            return -1;
        }
        if (type == CMDTYPE::SELECT) 
        {
            // get res
            while ((ret = dbresults(m_dbproc)) != NO_MORE_RESULTS) // have result or error
            {
                if (ret == FAIL) {
                    std::cout << "dbresults() failed" << std::endl;
                    return -1;
                }
                // get result col counts
                ncols = dbnumcols(m_dbproc);
                ResCol* cols = new ResCol[ncols];
                // get all cols
                //std::vector<std::string> colName;
                //colName.reserve(ncols);
                for (int i = 0; i < ncols; ++i)
                {
                    cols[i].name = dbcolname(m_dbproc, i + 1);
                    cols[i].type = dbcoltype(m_dbproc, i + 1);
                    cols[i].size = dbcollen(m_dbproc, i + 1); // not true
                    cols[i].buf = new char[BUFSIZE];
                    //colName.emplace_back(cols[i].name);
                    ret = dbbind(m_dbproc, i + 1, NTBSTRINGBIND, BUFSIZE, (BYTE*)cols[i].buf);
                    if (ret == FAIL) {
                        std::cout << "dbbind() failed" << std::endl;
                        goto DEL;
                    }
                    // A zero-length string is not a NULL! dbnullbind() arranges for the passed buffer to be set to -1 whenever that column is NULL for a particular row
                    ret = dbnullbind(m_dbproc, i + 1, &cols[i].status);
                    if (ret == FAIL) {
                        std::cout << "dbnullbind() failed" << std::endl;
                        goto DEL;
                    }
                }
                //res.emplace_back(colName);// push back col name
                // get all data
                while ((rowCode = dbnextrow(m_dbproc)) != NO_MORE_ROWS)
                {
                    std::vector<std::string> oneRow;
                    switch (rowCode)
                    {
                    case REG_ROW: // get data
                        for (int i = 0; i < ncols; ++i)
                        {
                            const char* str = (cols[i].status == -1 ? "" : cols[i].buf);
                            oneRow.emplace_back(str);
                        }
                        break;
                    case BUF_FULL:
                        std::cout << "dbnextrow() get BUF_FULL" << std::endl;
                        goto DEL;
                        break;
                    case FAIL:
                        std::cout << "dbnextrow() get FAIL" << std::endl;
                        goto DEL;
                        break;
                    default: // Computed rows are left as an exercise to the reader.
                        std::cout << "Data for computeid" << rowCode << "ignored" << std::endl;
                    }
                    if (!oneRow.empty()) {
                        res.emplace_back(oneRow);
                    }
                }
            DEL:
                // delete cache
                for (int i = 0; i < ncols; ++i)
                {
                    if (cols[i].buf) {
                        delete cols[i].buf;
                    }
                }
                delete[] cols;
            }
        }
        return ret == FAIL ? -1 : 0;
    }
    std::cout<< "m_dbproc is empty pointer!"<<std::endl;
    return -1;
}

int SqlServerHdl::MsgHandler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
    int changed_database = 5701, changed_language = 5703;
    if (msgno == changed_database || msgno == changed_language) {
        return 0;
    }
    if (msgno > 0) {
        printf("Msg %ld, Level %d, State %d\n", (long)msgno, severity, msgstate);
        if (strlen(srvname) > 0) {
            printf("Server '%s', ", srvname);
        }
        if (strlen(procname) > 0) {
            printf("Procedure '%s', ", procname);
        }
        if (line > 0) {
            printf("Line %d", line);
        }
        printf("\n");
    }
    printf("err msg: %s\n", msgtext);
    return 0;
}

int SqlServerHdl::ErrHandler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
    if(dberr) {
        printf("Msg %d, Level %d\n%s\n", dberr, severity, dberrstr);
    }
    else {
        printf("DB-LIBRARY error:\n%s\n", dberrstr);
    }
    return INT_CANCEL; // #define INT_CANCEL  2
}
