﻿// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/newstablemodel.h>

#include <chain.h>
#include <txdb.h>
#include <utilmoneystr.h>
#include <validation.h>

#include <qt/clientmodel.h>

#include <QDateTime>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(NewsTableObject)

NewsTableModel::NewsTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    nFilter = COIN_NEWS_ALL;
}

int NewsTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int NewsTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 4;
}

QVariant NewsTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<NewsTableObject>())
        return QVariant();

    NewsTableObject object = model.at(row).value<NewsTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Fees
        if (col == 0) {
            return QString::fromStdString(object.fees);
        }
        // Height
        if (col == 1) {
            return object.nHeight;
        }
        // Time
        if (col == 2) {
            return object.nTime;
        }
        // Decode
        if (col == 3) {
            return QString::fromStdString(object.decode);
        }

    }
    case Qt::TextAlignmentRole:
    {
        // Fees
        if (col == 0) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Height
        if (col == 1) {
            return int(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        // Time
        if (col == 2) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Decode
        if (col == 3) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    case NewsRole:
    {
        return QString::fromStdString(object.decode);
    }
    }
    return QVariant();
}

QVariant NewsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Fees");
            case 1:
                return QString("Height");
            case 2:
                return QString("Time");
            case 3:
                return QString("Decode");
            }
        }
    }
    return QVariant();
}

void NewsTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        numBlocksChanged();

        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged()));
    }
}

void NewsTableModel::numBlocksChanged()
{
    UpdateModel();
}

// TODO use this to initialize model / resync after filter change and then
// have a function to append new data to the model.
void NewsTableModel::UpdateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    int nHeight = chainActive.Height() + 1;

    int nBlocksToDisplay = 0;
    if (nFilter == COIN_NEWS_ALL) {
        nBlocksToDisplay = 24 * 6; // 6 blocks per hour * 24 hours
    }
    else
    if (nFilter == COIN_NEWS_TOKYO_DAY) {
        nBlocksToDisplay = 24 * 6; // 6 blocks per hour * 24 hours
    }
    else
    if (nFilter == COIN_NEWS_US_DAY) {
        nBlocksToDisplay = 24 * 6; // 6 blocks per hour * 24 hours
    }


    if (nHeight < nBlocksToDisplay)
        nBlocksToDisplay = nHeight;

    // Lookup and filter data that we want to display
    std::vector<NewsTableObject> vNews;
    for (int i = 0; i < nBlocksToDisplay; i++) {
        CBlockIndex *index = chainActive[nHeight - i];

        // TODO add error message or something to table?
        if (!index) {
            continue;
        }

        // For each block load our cached OP_RETURN data
        std::vector<OPReturnData> vData;
        if (!popreturndb->GetBlockData(index->GetBlockHash(), vData)) {
            continue;
        }

        // Find the data we want to display
        for (const OPReturnData& d : vData) {
            if (nFilter == COIN_NEWS_TOKYO_DAY && !d.script.IsNewsTokyoDay())
                continue;
            if (nFilter == COIN_NEWS_US_DAY && !d.script.IsNewsUSDay())
                continue;

            NewsTableObject object;
            object.nHeight = nHeight - i;
            object.nTime = index->nTime;

            std::string strDecode;
            for (const unsigned char& c : d.script)
                strDecode += c;

            object.decode = strDecode;
            object.fees = FormatMoney(d.fees);

            vNews.push_back(object);
        }
    }

    // Sort by fees
    SortByFees(vNews);

    beginInsertRows(QModelIndex(), model.size(), model.size() + vNews.size() - 1);
    for (const NewsTableObject& o : vNews)
        model.append(QVariant::fromValue(o));
    endInsertRows();
}

void NewsTableModel::setFilter(int nFilterIn)
{
    if (nFilterIn == COIN_NEWS_ALL) {
        nFilter = COIN_NEWS_ALL;
        UpdateModel();
    }
    else
    if (nFilterIn == COIN_NEWS_TOKYO_DAY) {
        nFilter = COIN_NEWS_TOKYO_DAY;
        UpdateModel();
    }
    else
    if (nFilterIn == COIN_NEWS_US_DAY) {
        nFilter = COIN_NEWS_US_DAY;
        UpdateModel();
    }
}

struct CompareByFee
{
    bool operator()(const NewsTableObject& a, const NewsTableObject& b) const
    {
        return a.fees > b.fees;
    }
};

void NewsTableModel::SortByFees(std::vector<NewsTableObject>& vNews)
{
    std::sort(vNews.begin(), vNews.end(), CompareByFee());
}