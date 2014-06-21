#!/usr/bin/env ruby

def getResult(cmd, input)
  pipe = IO.popen(cmd, 'r+')
  pipe.write(input)
  pipe.close_write
  o = pipe.read
  o
end

num_tests = 0
fails = []

ARGV.each do |test|
  test_path = "test/#{test}"
  tests = Dir.glob("#{test_path}*.in").sort
  tests = [test_path] if tests.empty?
  tests.each do |test_case|
    input = test_case == test_path ? '' : File.read(test_case)
    expected = getResult([test_path], input)
    actual = getResult(['./befunge', "#{test_path}.bef"], input)
    #actual = getResult(['./cfunge', "#{test_path}.bef"], input)
    if expected == actual
      puts "#{test_case}: OK (#{expected.sub(/\n.*/ms, '...')})"
    else
      puts "#{test_case}: FAIL expected=#{expected} actual=#{actual}"
      fails << test_case
    end
    num_tests += 1
  end
end

if fails.empty?
  puts 'PASS'
else
  puts "Failed tests:"
  puts fails.map{|f|f.inspect}
  puts "#{fails.size} / #{num_tests} FAIL"
end
