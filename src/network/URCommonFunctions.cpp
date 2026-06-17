/**
 * @file URCommonFunctions.cpp
 */
#include "multipack/network/URCommonFunctions.h"
#include "multipack/config/ConfigDefaults.h"
#include "multipack/core/GlobalState.h"
#include <QDebug>

namespace multipack { namespace network { namespace URCommonFunctions {

RpcValue getData(const QVector<RpcValue>& params)
{
    auto& state = core::GlobalState::instance();

    QVector<RpcValue> data;
    data.append(RpcValue::fromIntArray(state.paletteDimensions()));
    data.append(RpcValue::fromIntArray(state.packageDimensions()));
    data.append(RpcValue::fromIntArray({state.layerTypeCount()}));
    data.append(RpcValue::fromIntArray({state.numberOfLayers()}));

    if (params.isEmpty()) {
        return RpcValue::fromArray(data);
    }

    int index = params[0].toInt();
    if (index < 0 || index >= data.size()) {
        return RpcValue();
    }

    return data[index];
}

RpcValue getVersion(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    return RpcValue(QString(config::Defaults::VERSION));
}

}}} // namespace
