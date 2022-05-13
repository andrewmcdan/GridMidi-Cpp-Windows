const GridmidiGui = require("../lib/binding.js");
const assert = require("assert");

assert(GridmidiGui, "The expected function is undefined");

function testBasic()
{
    const result =  GridmidiGui("hello");
    assert.strictEqual(result, "world", "Unexpected value returned");
}

assert.doesNotThrow(testBasic, undefined, "testBasic threw an expection");

console.log("Tests passed- everything looks OK!");