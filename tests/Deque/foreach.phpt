--TEST--
Teds\Deque modification during foreach
--FILE--
<?php

/** Creates a reference-counted version of a literal string to test reference counting. */
function mut(string $s) {
    $s[0] = $s[0];
    return $s;
}

echo "Test push/unshift\n";
$deque = new Teds\Deque(['a', 'b']);
foreach ($deque as $key => $value) {
    if (strlen($value) === 1) {
        $deque->push("${value}_");
        $deque->unshift("_${value}");
    }
    printf("Key: %s Value: %s\n", var_export($key, true), var_export($value, true));
}
var_dump($deque);
echo "Test shift\n";
foreach ($deque as $key => $value) {
    echo "Shifting $key $value\n";
    var_dump($deque->shift());
}

echo "Test shift out of bounds\n";
$deque = new Teds\Deque([mut('a1'), mut('b1'), mut('c1')]);
foreach ($deque as $key => $value) {
    $deque->shift();
    $deque->shift();
    echo "Saw $key: $value\n";
    // iteration stops early because iteration position is now out of bounds.
}
var_dump($deque);

echo "Test iteration behavior\n";
$deque = new Teds\Deque([mut('a1'), mut('a2')]);
$it = $deque->getIterator();
echo json_encode(['valid' => $it->valid(), 'key' => $it->key(), 'value' => $it->current()]), "\n";
$deque->shift();
// invalid, outside the range of the deque
echo json_encode(['valid' => $it->valid(), 'key' => $it->key()]), "\n";
$it->next();
echo json_encode(['valid' => $it->valid(), 'key' => $it->key(), 'value' => $it->current()]), "\n";
$deque->unshift('a', 'b');
unset($deque);
echo json_encode(['valid' => $it->valid(), 'key' => $it->key(), 'value' => $it->current()]), "\n";

?>
--EXPECT--
Test push/unshift
Key: 0 Value: 'a'
Key: 2 Value: 'b'
Key: 4 Value: 'a_'
Key: 5 Value: 'b_'
object(Teds\Deque)#1 (6) {
  [0]=>
  string(2) "_b"
  [1]=>
  string(2) "_a"
  [2]=>
  string(1) "a"
  [3]=>
  string(1) "b"
  [4]=>
  string(2) "a_"
  [5]=>
  string(2) "b_"
}
Test shift
Shifting 0 _b
string(2) "_b"
Shifting 0 _a
string(2) "_a"
Shifting 0 a
string(1) "a"
Shifting 0 b
string(1) "b"
Shifting 0 a_
string(2) "a_"
Shifting 0 b_
string(2) "b_"
Test shift out of bounds
Saw 0: a1
object(Teds\Deque)#2 (1) {
  [0]=>
  string(2) "c1"
}
Test iteration behavior
{"valid":true,"key":0,"value":"a1"}
{"valid":false,"key":null}
{"valid":true,"key":0,"value":"a2"}
{"valid":true,"key":2,"value":"a2"}
