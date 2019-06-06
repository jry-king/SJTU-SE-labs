package sjtu.sdic.mapreduce;

import org.apache.commons.io.filefilter.WildcardFileFilter;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.core.Master;
import sjtu.sdic.mapreduce.core.Worker;

import java.io.File;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import java.util.TreeSet;

/**
 * Created by Cachhe on 2019/4/24.
 */
public class InvertedIndex {

    public static List<KeyValue> mapFunc(String file, String value)
    {
        // find all legal words, for each word add a KeyValue pair into result
        // specifically (word, file), to denote occurrence of the word in the file
        // redundant words are not eliminated here
        Pattern pattern = Pattern.compile("[a-zA-Z0-9]+");
        Matcher matcher = pattern.matcher(value);
        List<KeyValue> words = new ArrayList<>();
        while (matcher.find())
        {
            words.add(new KeyValue(matcher.group(), file));
        }
        return words;
    }

    public static String reduceFunc(String key, String[] values)
    {
        // here use TreeSet to ensure eliminate identical values and sort the rest ones
        TreeSet<String> resultSet = new TreeSet<>(new ArrayList<>(Arrays.asList(values)));
        // construct result string
        StringBuffer result = new StringBuffer(String.format("%d ", resultSet.size()));
        for (String s:resultSet)
        {
            result.append(s+",");
        }
        result.delete(result.length()-1, result.length());
        return result.toString();
    }

    public static void main(String[] args) {
        if (args.length < 3) {
            System.out.println("error: see usage comments in file");
        } else if (args[0].equals("master")) {
            Master mr;

            String src = args[2];
            File file = new File(".");
            String[] files = file.list(new WildcardFileFilter(src));
            if (args[1].equals("sequential")) {
                mr = Master.sequential("iiseq", files, 3, InvertedIndex::mapFunc, InvertedIndex::reduceFunc);
            } else {
                mr = Master.distributed("wcdis", files, 3, args[1]);
            }
            mr.mWait();
        } else {
            Worker.runWorker(args[1], args[2], InvertedIndex::mapFunc, InvertedIndex::reduceFunc, 100, null);
        }
    }
}
