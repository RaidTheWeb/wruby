assert '_protect' do
  # no failure in protect returns [result, false]
  assert_equal ['test', false] do
    ExceptionTest._protect { 'test' }
  end
  # failure in protect returns [exception, true]
  result = ExceptionTest._protect { raise 'test' }
  assert_kind_of RuntimeError, result[0]
  assert_true result[1]
end

assert '_ensure' do
  a = false
  assert_equal 'test' do
    ExceptionTest._ensure Proc.new { 'test' }, Proc.new { a = true }
  end
  assert_true a

  a = false
  assert_raise RuntimeError do
    ExceptionTest._ensure Proc.new { raise 'test' }, Proc.new { a = true }
  end
  assert_true a
end

assert '_rescue' do
  assert_equal 'test' do
    ExceptionTest._rescue Proc.new { 'test' }, Proc.new {}
  end

  class CustomExp < Exception
  end

  assert_raise CustomExp do
    ExceptionTest._rescue Proc.new { raise CustomExp.new 'test' }, Proc.new { 'rescue' }
  end

  assert_equal 'rescue' do
    ExceptionTest._rescue Proc.new { raise 'test' }, Proc.new { 'rescue' }
  end
end

assert '_rescue_exceptions' do
  assert_equal 'test' do
    ExceptionTest._rescue_exceptions Proc.new { 'test' }, Proc.new {}
  end

  assert_raise RangeError do
    ExceptionTest._rescue_exceptions Proc.new { raise RangeError.new 'test' }, Proc.new { 'rescue' }
  end

  assert_equal 'rescue' do
    ExceptionTest._rescue_exceptions Proc.new { raise TypeError.new 'test' }, Proc.new { 'rescue' }
  end
end
