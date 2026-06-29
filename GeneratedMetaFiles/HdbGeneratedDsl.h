// 此文件由 ysd_hdb_meta_codegen 生成，请勿手动修改
#ifndef YSD_HDB_GENERATED_DSL_H
#define YSD_HDB_GENERATED_DSL_H

#include "../dll/HdbQueryBuilder.h"

namespace HdbDsl
{

struct CHdbGeneratedAlarmTable : public CHdbDslTable
{
    CHdbDslField ID;
    CHdbDslField POINT_ID;
    CHdbDslField LEVEL;
    CHdbDslField MESSAGE;
    CHdbDslField OCCUR_TIME;

    CHdbGeneratedAlarmTable()
        : CHdbDslTable("alarm"),
          ID("alarm", "id", HDB_FT_INT64),
          POINT_ID("alarm", "point_id", HDB_FT_INT64),
          LEVEL("alarm", "level", HDB_FT_INT32),
          MESSAGE("alarm", "message", HDB_FT_CHAR_ARRAY),
          OCCUR_TIME("alarm", "occur_time", HDB_FT_TIMESTAMP_MS)
    {
    }
};

static const CHdbGeneratedAlarmTable ALARM;

struct AlarmRow
{
    HdbInt64 id;
    HdbInt64 point_id;
    int level;
    std::string message;
    HdbInt64 occur_time;

    int FillFrom(CHdbDslResult& result)
    {
        int ret;

        ret = result.Get(ALARM.ID, id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(ALARM.POINT_ID, point_id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(ALARM.LEVEL, level);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(ALARM.MESSAGE, message);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(ALARM.OCCUR_TIME, occur_time);
        if (ret != HDB_OK)
        {
            return ret;
        }
        return HDB_OK;
    }
};

struct CHdbGeneratedDeviceTable : public CHdbDslTable
{
    CHdbDslField ID;
    CHdbDslField NAME;

    CHdbGeneratedDeviceTable()
        : CHdbDslTable("device"),
          ID("device", "id", HDB_FT_INT64),
          NAME("device", "name", HDB_FT_CHAR_ARRAY)
    {
    }
};

static const CHdbGeneratedDeviceTable DEVICE;

struct DeviceRow
{
    HdbInt64 id;
    std::string name;

    int FillFrom(CHdbDslResult& result)
    {
        int ret;

        ret = result.Get(DEVICE.ID, id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(DEVICE.NAME, name);
        if (ret != HDB_OK)
        {
            return ret;
        }
        return HDB_OK;
    }
};

struct CHdbGeneratedModelCrudTestTable : public CHdbDslTable
{
    CHdbDslField ID;
    CHdbDslField TYPE;
    CHdbDslField NAME;
    CHdbDslField CREATE_TIME;

    CHdbGeneratedModelCrudTestTable()
        : CHdbDslTable("model_crud_test"),
          ID("model_crud_test", "id", HDB_FT_INT64),
          TYPE("model_crud_test", "type", HDB_FT_INT32),
          NAME("model_crud_test", "name", HDB_FT_CHAR_ARRAY),
          CREATE_TIME("model_crud_test", "create_time", HDB_FT_TIMESTAMP_MS)
    {
    }
};

static const CHdbGeneratedModelCrudTestTable MODEL_CRUD_TEST;

struct ModelCrudTestRow
{
    HdbInt64 id;
    int type;
    std::string name;
    HdbInt64 create_time;

    int FillFrom(CHdbDslResult& result)
    {
        int ret;

        ret = result.Get(MODEL_CRUD_TEST.ID, id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(MODEL_CRUD_TEST.TYPE, type);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(MODEL_CRUD_TEST.NAME, name);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(MODEL_CRUD_TEST.CREATE_TIME, create_time);
        if (ret != HDB_OK)
        {
            return ret;
        }
        return HDB_OK;
    }
};

struct CHdbGeneratedPointTable : public CHdbDslTable
{
    CHdbDslField ID;
    CHdbDslField DEVICE_ID;
    CHdbDslField NAME;

    CHdbGeneratedPointTable()
        : CHdbDslTable("point"),
          ID("point", "id", HDB_FT_INT64),
          DEVICE_ID("point", "device_id", HDB_FT_INT64),
          NAME("point", "name", HDB_FT_CHAR_ARRAY)
    {
    }
};

static const CHdbGeneratedPointTable POINT;

struct PointRow
{
    HdbInt64 id;
    HdbInt64 device_id;
    std::string name;

    int FillFrom(CHdbDslResult& result)
    {
        int ret;

        ret = result.Get(POINT.ID, id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(POINT.DEVICE_ID, device_id);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = result.Get(POINT.NAME, name);
        if (ret != HDB_OK)
        {
            return ret;
        }
        return HDB_OK;
    }
};

}

#endif
