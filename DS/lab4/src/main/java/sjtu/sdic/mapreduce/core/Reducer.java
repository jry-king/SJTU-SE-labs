package sjtu.sdic.mapreduce.core;

import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import javafx.scene.input.KeyCharacterCombination;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.common.Utils;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

/**
 * Created by Cachhe on 2019/4/19.
 */
public class Reducer {

    /**
     * 
     * 	doReduce manages one reduce task: it should read the intermediate
     * 	files for the task, sort the intermediate key/value pairs by key,
     * 	call the user-defined reduce function {@code reduceF} for each key,
     * 	and write reduceF's output to disk.
     * 	
     * 	You'll need to read one intermediate file from each map task;
     * 	{@code reduceName(jobName, m, reduceTask)} yields the file
     * 	name from map task m.
     *
     * 	Your {@code doMap()} encoded the key/value pairs in the intermediate
     * 	files, so you will need to decode them. If you used JSON, you can refer
     * 	to related docs to know how to decode.
     * 	
     *  In the original paper, sorting is optional but helpful. Here you are
     *  also required to do sorting. Lib is allowed.
     * 	
     * 	{@code reduceF()} is the application's reduce function. You should
     * 	call it once per distinct key, with a slice of all the values
     * 	for that key. {@code reduceF()} returns the reduced value for that
     * 	key.
     * 	
     * 	You should write the reduce output as JSON encoded KeyValue
     * 	objects to the file named outFile. We require you to use JSON
     * 	because that is what the merger then combines the output
     * 	from all the reduce tasks expects. There is nothing special about
     * 	JSON -- it is just the marshalling format we chose to use.
     * 	
     * 	Your code here (Part I).
     * 	
     * 	
     * @param jobName the name of the whole MapReduce job
     * @param reduceTask which reduce task this is
     * @param outFile write the output here
     * @param nMap the number of map tasks that were run ("M" in the paper)
     * @param reduceF user-defined reduce function
     */
    public static void doReduce(String jobName, int reduceTask, String outFile, int nMap, ReduceFunc reduceF)
    {
        // read all intermediate files belonging to this reduce task
        List<KeyValue> keyValues = new ArrayList<>();
        for (int i = 0; i < nMap; i++)
        {
            String text = "";
            try
            {
                text = new String(Files.readAllBytes(new File(Utils.reduceName(jobName, i, reduceTask)).toPath()));
            }
            catch (IOException e)
            {
                e.printStackTrace();
            }
            // decode those strings into arrayLists by JSONArray.parseArray
            keyValues.addAll(JSONArray.parseArray(text, KeyValue.class));
        }

        // sort the whole list by key
        Collections.sort(keyValues, new Comparator<KeyValue>() {
            @Override
            public int compare(KeyValue o1, KeyValue o2) {
                return o1.key.compareTo(o2.key);
            }
        });

        // call the user-defined reduce function for each key
        // store results of reduce operation in a JSONObject
        // the algorithm is, from the second pair on, compare every key with that of the former pair
        // if identical, add the pair's value into tempValues
        // otherwise perform reduceF over the key and all those values stored
        // and store the result afterwards
        String tempKey = keyValues.get(0).key;
        List<String> tempValues = new ArrayList<>();
        tempValues.add(keyValues.get(0).value);
        JSONObject result = new JSONObject();
        for (int i = 1; i < keyValues.size(); i++)
        {
            if (keyValues.get(i).key.equals(tempKey))
            {
                tempValues.add(keyValues.get(i).value);
            }
            else
            {
                result.put(tempKey, reduceF.reduce(tempKey, tempValues.toArray(new String[tempValues.size()])));
                tempKey = keyValues.get(i).key;
                tempValues.clear();
                tempValues.add(keyValues.get(i).value);
            }
        }
        result.put(tempKey, reduceF.reduce(tempKey, tempValues.toArray(new String[tempValues.size()])));

        // write the result to result file
        File resFile = new File(outFile);
        String resString = JSONObject.toJSONString(result);
        try
        {
            Files.write(resFile.toPath(), resString.getBytes());
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }
}
