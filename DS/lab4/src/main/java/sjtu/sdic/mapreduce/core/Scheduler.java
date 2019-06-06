package sjtu.sdic.mapreduce.core;

import sjtu.sdic.mapreduce.common.Channel;
import sjtu.sdic.mapreduce.common.DoTaskArgs;
import sjtu.sdic.mapreduce.common.JobPhase;
import sjtu.sdic.mapreduce.common.Utils;
import sjtu.sdic.mapreduce.rpc.Call;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;

import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.TimeUnit;

/**
 * Created by Cachhe on 2019/4/22.
 */
public class Scheduler {
    // use a vector, which is thread-safe, to record whether each task is finished
    private static CopyOnWriteArrayList<Integer> taskState;

    /**
     * schedule() starts and waits for all tasks in the given phase (mapPhase
     * or reducePhase). the mapFiles argument holds the names of the files that
     * are the inputs to the map phase, one per map task. nReduce is the
     * number of reduce tasks. the registerChan argument yields a stream
     * of registered workers; each item is the worker's RPC address,
     * suitable for passing to {@link Call}. registerChan will yield all
     * existing registered workers (if any) and new ones as they register.
     *
     * @param jobName job name
     * @param mapFiles files' name (if in same dir, it's also the files' path)
     * @param nReduce the number of reduce task that will be run ("R" in the paper)
     * @param phase MAP or REDUCE
     * @param registerChan register info channel
     */
    public static void schedule(String jobName, String[] mapFiles, int nReduce, JobPhase phase, Channel<String> registerChan)
    {
        int nTasks = -1; // number of map or reduce tasks
        int nOther = -1; // number of inputs (for reduce) or outputs (for map)
        switch (phase) {
            case MAP_PHASE:
                nTasks = mapFiles.length;
                nOther = nReduce;
                break;
            case REDUCE_PHASE:
                nTasks = nReduce;
                nOther = mapFiles.length;
                break;
        }

        System.out.println(String.format("Schedule: %d %s tasks (%d I/Os)", nTasks, phase, nOther));

        /**
        // All ntasks tasks have to be scheduled on workers. Once all tasks
        // have completed successfully, schedule() should return.
        //
        // Your code here (Part III, Part IV).
        //
        */
        // initialize the vector with all task indexes and delete one index once corresponding task is finished
        taskState = new CopyOnWriteArrayList<>();
        for (int i = 0; i < nTasks; i++)
        {
            taskState.add(new Integer(i));
        }
        // use a CountDownLatch to wait for all tasks to complete
        CountDownLatch countDownLatch = new CountDownLatch(nTasks);
        // to ensure a worker can be assigned a new task only after finishing current task, the worker will be
        // removed from register channel once assigned, and registered again once finishing task
        String worker;
        while (!taskState.isEmpty())
        {
            try
            {
                for (Integer i:taskState)
                {
                    worker = registerChan.read();
                    DoTaskArgs doTaskArgs = new DoTaskArgs(jobName, mapFiles[i], phase, i, nOther);
                    WorkerThread workerThread = new WorkerThread(doTaskArgs, worker, countDownLatch, registerChan);
                    workerThread.start();
                }
                countDownLatch.await(5, TimeUnit.SECONDS);      // set timeout to 5 seconds
            }
            catch (InterruptedException e)
            {
                e.printStackTrace();
            }
        }

        System.out.println(String.format("Schedule: %s done", phase));
    }

    // the tasks are actually done concurrently in RPC calls in worker threads
    private static class WorkerThread extends Thread
    {
        private DoTaskArgs doTaskArgs;              // RPC arguments
        private String worker;                      // name (RPC address) of the worker
        private CountDownLatch countDownLatch;
        private Channel<String> registerChan;

        WorkerThread(DoTaskArgs doTaskArgs, String worker, CountDownLatch countDownLatch, Channel<String> registerChan)
        {
            this.doTaskArgs = doTaskArgs;
            this.worker = worker;
            this.countDownLatch = countDownLatch;
            this.registerChan = registerChan;
        }

        @Override
        public void run()
        {
            Call.getWorkerRpcService(this.worker).doTask(this.doTaskArgs);
            taskState.remove(Integer.valueOf(this.doTaskArgs.taskNum));
            countDownLatch.countDown();
            try
            {
                registerChan.write(this.worker);
            }
            catch (InterruptedException e)
            {
                e.printStackTrace();
            }
        }
    }
}
