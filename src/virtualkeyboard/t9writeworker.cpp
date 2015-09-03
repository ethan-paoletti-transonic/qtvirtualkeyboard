/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://qt.io
**
** This file is part of the Qt Virtual Keyboard module.
**
** Licensees holding valid commercial license for Qt may use this file in
** accordance with the Qt License Agreement provided with the Software
** or, alternatively, in accordance with the terms contained in a written
** agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.io
**
******************************************************************************/

#include "t9writeworker.h"
#include "virtualkeyboarddebug.h"

#include <QFile>
#include <QTime>

T9WriteTask::T9WriteTask(QObject *parent) :
    QObject(parent),
    decumaSession(0),
    runSema()
{
}

void T9WriteTask::wait()
{
    runSema.acquire();
    runSema.release();
}

T9WriteDictionaryTask::T9WriteDictionaryTask(const QString &fileUri,
                                             const DECUMA_MEM_FUNCTIONS &memFuncs) :
    fileUri(fileUri),
    memFuncs(memFuncs)
{
}

void T9WriteDictionaryTask::run()
{
    VIRTUALKEYBOARD_DEBUG() << "T9WriteDictionaryTask::run()";

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif

    void *dictionary = 0;

    QFile dictionaryFile(fileUri);
    if (dictionaryFile.open(QIODevice::ReadOnly)) {
        uchar *dictionaryData = dictionaryFile.map(0, dictionaryFile.size(), QFile::NoOptions);
        if (dictionaryData) {

            DECUMA_SRC_DICTIONARY_INFO dictionaryInfo;
            memset(&dictionaryInfo, 0, sizeof(dictionaryInfo));
            dictionaryInfo.srcType = decumaXT9LDB;
            DECUMA_UINT32 dictionarySize = 0;
            DECUMA_STATUS status = decumaConvertDictionary(&dictionary, dictionaryData, dictionaryFile.size(), &dictionaryInfo, &dictionarySize, &memFuncs);
            Q_UNUSED(status)
            Q_ASSERT(status == decumaNoError);
            dictionaryFile.unmap(dictionaryData);
        } else {
            qWarning() << "Could not map dictionary file" << fileUri;
        }
    } else {
        qWarning() << "Could not open dictionary file" << fileUri;
    }

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    VIRTUALKEYBOARD_DEBUG() << "T9WriteDictionaryTask::run(): time:" << perf.elapsed() << "ms";
#endif

    emit completed(fileUri, dictionary);
}

T9WriteRecognitionResult::T9WriteRecognitionResult(int id, int maxResults, int maxCharsPerWord) :
    numResults(0),
    instantGesture(0),
    id(id),
    maxResults(maxResults),
    maxCharsPerWord(maxCharsPerWord)
{
    Q_ASSERT(maxResults > 0);
    Q_ASSERT(maxCharsPerWord > 0);
    results.resize(maxResults);
    int bufferLength = (maxCharsPerWord + 1);
    _chars.resize(maxResults * bufferLength);
    _symbolChars.resize(maxResults * bufferLength);
    _symbolStrokes.resize(maxResults * bufferLength);
    for (int i = 0; i < maxResults; i++) {
        DECUMA_HWR_RESULT &hwrResult = results[i];
        hwrResult.pChars = &_chars[i * bufferLength];
        hwrResult.pSymbolChars = &_symbolChars[i * bufferLength];
        hwrResult.pSymbolStrokes = &_symbolStrokes[i * bufferLength];
    }
}

T9WriteRecognitionTask::T9WriteRecognitionTask(QSharedPointer<T9WriteRecognitionResult> result,
                                               const DECUMA_INSTANT_GESTURE_SETTINGS &instantGestureSettings,
                                               BOOST_LEVEL boostLevel,
                                               const QString &stringStart) :
    T9WriteTask(),
    result(result),
    instantGestureSettings(instantGestureSettings),
    boostLevel(boostLevel),
    stringStart(stringStart),
    stateCancelled(false)
{
    VIRTUALKEYBOARD_DEBUG() << "T9WriteRecognitionTask():" << "boostLevel:" << boostLevel << "stringStart:" << stringStart;
}

void T9WriteRecognitionTask::run()
{
    VIRTUALKEYBOARD_DEBUG() << "T9WriteRecognitionTask::run()";

    if (!decumaSession)
        return;

    {
        QMutexLocker stateGuard(&stateLock);
        Q_UNUSED(stateGuard);
        if (stateCancelled)
            return;
    }

    //In a normal text composition case boostDictWords and canBeContinued are the preffered settings
    DECUMA_RECOGNITION_SETTINGS recSettings;
    memset(&recSettings, 0, sizeof(recSettings));
    recSettings.boostLevel = boostLevel;
    recSettings.stringCompleteness = canBeContinued;
    if (!stringStart.isEmpty())
        recSettings.pStringStart = (DECUMA_UNICODE *)stringStart.utf16();

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif

    DECUMA_STATUS status = decumaIndicateInstantGesture(decumaSession, &result->instantGesture, &instantGestureSettings);
    Q_ASSERT(status == decumaNoError);

    DECUMA_INTERRUPT_FUNCTIONS interruptFunctions;
    interruptFunctions.pShouldAbortRecognize = shouldAbortRecognize;
    interruptFunctions.pUserData = (void *)this;
    status = decumaRecognize(decumaSession, result->results.data(), result->results.size(), &result->numResults, result->maxCharsPerWord, &recSettings, &interruptFunctions);
    if (status == decumaAbortRecognitionUnsupported)
        status = decumaRecognize(decumaSession, result->results.data(), result->results.size(), &result->numResults, result->maxCharsPerWord, &recSettings, NULL);
    Q_ASSERT(status == decumaNoError);

    QStringList resultList;
    QString gesture;
    for (int i = 0; i < result->numResults; i++)
    {
        QString resultString;
        resultString.reserve(result->results[i].nChars);
        int charPos = 0;
        for (int symbolIndex = 0; symbolIndex < result->results[i].nSymbols; symbolIndex++) {
            int symbolLength = result->results[i].pSymbolChars[symbolIndex];
            QString symbol(QString::fromUtf16(&result->results[i].pChars[charPos], symbolLength));
            // Do not append gesture symbol to result string
            if (result->results[i].bGesture && symbolIndex == (result->results[i].nSymbols - 1)) {
                if (i == 0 && (result->instantGesture || (symbol != QLatin1String(" ") && symbol != QLatin1String("\b"))))
                    gesture = symbol;
            } else {
                resultString.append(symbol);
            }
            charPos += symbolLength;
        }
        resultList.append(resultString);
    }

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    int perfElapsed = perf.elapsed();
#endif

    {
        QMutexLocker stateGuard(&stateLock);
        Q_UNUSED(stateGuard)
        if (stateCancelled)
            result.reset();
#ifdef QT_VIRTUALKEYBOARD_DEBUG
        VIRTUALKEYBOARD_DEBUG() << "T9WriteRecognitionTask::run(): time:" << perfElapsed << "ms" << (stateCancelled ? "(cancelled)" : "");
#endif
    }
}

int T9WriteRecognitionTask::shouldAbortRecognize(void *pUserData)
{
    T9WriteRecognitionTask *pThis = (T9WriteRecognitionTask *)pUserData;
    QMutexLocker stateGuard(&pThis->stateLock);
    Q_UNUSED(stateGuard)
    return pThis->stateCancelled;
}

bool T9WriteRecognitionTask::cancelRecognition()
{
    QMutexLocker stateGuard(&stateLock);
    Q_UNUSED(stateGuard)
    stateCancelled = true;
    return true;
}

int T9WriteRecognitionTask::resultId() const
{
    return result != 0 ? result->id : -1;
}

T9WriteRecognitionResultsTask::T9WriteRecognitionResultsTask(QSharedPointer<T9WriteRecognitionResult> result) :
    T9WriteTask(),
    result(result)
{
}

void T9WriteRecognitionResultsTask::run()
{
    if (!result)
        return;

    QVariantList resultList;
    for (int i = 0; i < result->numResults; i++)
    {
        QVariantMap resultMap;
        QString resultString;
        QString gesture;
        const DECUMA_HWR_RESULT &hwrResult = result->results.at(i);
        resultString.reserve(hwrResult.nChars);
        QVariantList symbolStrokes;
        int charPos = 0;
        for (int symbolIndex = 0; symbolIndex < hwrResult.nSymbols; symbolIndex++) {
            int symbolLength = hwrResult.pSymbolChars[symbolIndex];
            QString symbol(QString::fromUtf16(&hwrResult.pChars[charPos], symbolLength));
            // Do not append gesture symbol to result string
            if (hwrResult.bGesture && symbolIndex == (hwrResult.nSymbols - 1)) {
                if (result->instantGesture || (symbol != QLatin1String(" ") && symbol != QLatin1String("\b")))
                    gesture = symbol;
            } else {
                resultString.append(symbol);
            }
            charPos += symbolLength;
            if (hwrResult.pSymbolStrokes)
                symbolStrokes.append(QVariant((int)hwrResult.pSymbolStrokes[symbolIndex]));
        }

        resultMap["resultId"] = result->id;
        resultMap["chars"] = resultString;
        resultMap["symbolStrokes"] = symbolStrokes;
        if (!gesture.isEmpty())
            resultMap["gesture"] = gesture;

        resultList.append(resultMap);
    }

    if (resultList.isEmpty())
        return;

    emit resultsAvailable(resultList);
}

T9WriteWorker::T9WriteWorker(DECUMA_SESSION *decumaSession, QObject *parent) :
    QThread(parent),
    taskSema(),
    taskLock(),
    decumaSession(decumaSession),
    abort(false)
{
}

T9WriteWorker::~T9WriteWorker()
{
    abort = true;
    taskSema.release();
    wait();
}

void T9WriteWorker::addTask(QSharedPointer<T9WriteTask> task)
{
    if (task) {
        QMutexLocker guard(&taskLock);
        task->moveToThread(this);
        taskList.append(task);
        taskSema.release();
    }
}

int T9WriteWorker::removeTask(QSharedPointer<T9WriteTask> task)
{
    int count = 0;
    if (task) {
        QMutexLocker guard(&taskLock);
        count = taskList.removeAll(task);
        taskSema.acquire(qMin(count, taskSema.available()));
    }
    return count;
}

int T9WriteWorker::removeAllTasks()
{
    QMutexLocker guard(&taskLock);
    int count = taskList.count();
    taskList.clear();
    if (taskSema.available())
        taskSema.acquire(taskSema.available());
    return count;
}

void T9WriteWorker::run()
{
    while (!abort) {
        taskSema.acquire();
        if (abort)
            break;
        QSharedPointer<T9WriteTask> currentTask;
        {
            QMutexLocker guard(&taskLock);
            if (!taskList.isEmpty()) {
                currentTask = taskList.front();
                taskList.pop_front();
            }
        }
        if (currentTask) {
            currentTask->decumaSession = decumaSession;
            currentTask->run();
            currentTask->runSema.release();
        }
    }
}