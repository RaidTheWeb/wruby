assert 'protect' do
  # no failure in protect returns [result, false]
  assert_equal ['test', false] do
    ExceptionTest.protect { 'test' }
  end
  # failure in protect returns [exception, true]
  result = ExceptionTest.protect { raise 'test' }
  assert_kind_of RuntimeError, result[0]
  assert_true result[1]
end

assert 'ensure' do
  a = false
  assert_equal 'test' do
    ExceptionTest.ensure Proc.new { 'test' }, Proc.new { a = true }
  end
  assert_true a

  a = false
  assert_raise RuntimeError do
    ExceptionTest.ensure Proc.new { raise 'test' }, Proc.new { a = true }
  end
  assert_true a
end

assert 'rescue' do
  assert_equal 'test' do
    ExceptionTest.rescue Proc.new { 'test' }, Proc.new {}
  end

  class CustomExp < Exception
  end

  assert_raise CustomExp do
    ExceptionTest.rescue Proc.new { raise CustomExp.new 'test' }, Proc.new { 'rescue' }
  end

  assert_equal 'rescue' do
    ExceptionTest.rescue Proc.new { raise 'test' }, Proc.new { 'rescue' }
  end
end

assert 'rescue_exceptions' do
  assert_equal 'test' do
    ExceptionTest.rescue_exceptions Proc.new { 'test' }, Proc.new {}
  end

  assert_raise RangeError do
    ExceptionTest.rescue_exceptions Proc.new { raise RangeError.new 'test' }, Proc.new { 'rescue' }
  end

  assert_equal 'rescue' do
    ExceptionTest.rescue_exceptions Proc.new { raise TypeError.new 'test' }, Proc.new { 'rescue' }
  end
end
