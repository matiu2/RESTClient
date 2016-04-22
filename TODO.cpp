// REST API Layer, runs on top of HTTP, and understands JSON

/// For example 'blog_post', any requests done with this would have '/blog_post' in the path
class RESTResourceType {
public:
  RESTResourceType(std::string name);
  // Would do PUT or POST on the server
  std::shared_ptr<RESTResource> create(JDict data); 
  std::vector<shared_ptr<RESTResource> list(int start_page, int page_size);
};

// An instance of a resource
class RESTResource {
public:
  RESTResourceType& type;
  std::string id;
  std::string get();
  JDict data;
  // Would call for example PUT /blog_post/4
  void update(std::string data);
  void del();
};

void example() {
  HTTP server("httpbin.org");
  // Define the types of resources you'll be dealing with
  RESTResourceType blog_post_type("blog_post");
  RESTResourceType person_type("person");
  // Create a blog post
  std::shared_ptr<RestResource> post1 =
      blog_post_type.create({{"title", "my post"}, {"body", "blah blah"}});
  // Make a change to the blog post
  post1->data["title"] = "Better Title";
  // Push that change to the server
  post1->update();
  // Delete the blog post (on the server)
  post1->del();
  // List the first 50 people
  auto people = person_type.list(1, 50);
  // Find a person named 'joe' and give him a payrise
  for (auto person: people)
    if (person->data.at("name") == "joe") {
      person->data["pay_rise"] = true;
      person->update();
    }
}
